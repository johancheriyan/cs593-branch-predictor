import csv
import random
import os
import json
from datetime import datetime
from deap import base, creator, tools

# ================================================================
# POINT THIS TO YOUR EXISTING RUN
# ================================================================
RESUME_FROM = "runs/20260421_095200"   # ← change to your actual run folder

# ================================================================
# PARAMS & OBJECTIVES (must match original run)
# ================================================================
PARAMS = [
    {"name": "LOGLB",  "min": 4,  "max": 10}, #
    {"name": "NUMG",   "min": 4,  "max": 8},
    {"name": "LOGG",   "min": 8,  "max": 12},
    {"name": "LOGB",   "min": 8,  "max": 14},
    {"name": "TAGW",   "min": 8,  "max": 14},
    {"name": "GHIST",  "min": 50, "max": 200},
    {"name": "LOGP1",  "min": 10, "max": 20},
    {"name": "GHIST1", "min": 4,  "max": 12}
]

OBJECTIVES = [
    {"name": "mispredictions",         "minimize": True},
    {"name": "p2 latency",             "minimize": True},
    {"name": "energy per instruction", "minimize": True}
]

# ================================================================
# STEP 1: Load existing evaluations into a cache
#         key   = tuple of params
#         value = tuple of objective values
# ================================================================
cache = {}

csv_path = os.path.join(RESUME_FROM, "all_evaluations.csv")
loaded = 0
skipped = 0

with open(csv_path, "r") as f:
    reader = csv.DictReader(f)
    for row in reader:
        try:
            key = tuple(int(row[p["name"]]) for p in PARAMS)
            val = tuple(float(row[o["name"]]) for o in OBJECTIVES)

            # Skip failed evaluations (inf values)
            if any(v == float('inf') for v in val):
                skipped += 1
                continue

            cache[key] = val
            loaded += 1
        except (ValueError, KeyError):
            skipped += 1

print(f"Loaded {loaded} cached evaluations ({skipped} skipped due to errors)")

# ================================================================
# STEP 2: Simulator that checks cache first
# ================================================================
import subprocess

call_counter  = [0]   # how many times we actually ran the simulator
cache_counter = [0]   # how many times we used cache
current_gen   = [0]
def simulate(params):
    key = tuple(params)
    if key in cache:
        cache_counter[0] += 1
        return list(cache[key])

    print(f"Evaluating gen {current_gen[0]:3d} | params: {params} ... ", end="", flush=True)
    call_counter[0] += 1

    # Build the predictor string separately for clarity
    predictor = f"tage<{params[0]},{params[1]},{params[2]},{params[3]},{params[4]},{params[5]},{params[6]},{params[7]}>"

    cmd = (
        f'./compile cbp '
        f'"-DPREDICTOR={predictor}" '
        f'&& ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human'
    )

    # ── Print the exact command so we can verify quoting ──────
    print(f"\n  CMD: {cmd}", flush=True)

    result = subprocess.run(cmd, capture_output=True, text=True, shell=True)

    if result.returncode != 0:
        print(f"FAILED", flush=True)
        print(f"  stderr: {result.stderr[:500]}", flush=True)
        return [float('inf')] * len(OBJECTIVES)

    values = []
    for line in result.stdout.splitlines():
        if "mispredictions" in line:
            if "short mispredictions" in line:
                continue
            values.append(int(line.split(":")[1].strip()))
        if "energy per instruction" in line:
            values.append(float(line.split(":")[1].strip().split()[0]))
        if "p2 latency" in line:
            values.append(float(line.split(":")[1].strip().split()[0]))

    if len(values) != len(OBJECTIVES):
        print(f"PARSE FAILED — got {len(values)} values, expected {len(OBJECTIVES)}", flush=True)
        print(f"  stdout: {result.stdout[:500]}", flush=True)
        return [float('inf')] * len(OBJECTIVES)

    cache[key] = tuple(values)
    log_evaluation(current_gen[0], params, values)
    print(" | ".join(f"{o['name']}: {v}" for o, v in zip(OBJECTIVES, values)), flush=True)
    return values

# ================================================================
# STEP 3: New logging setup (separate folder from original run)
# ================================================================
RUN_ID  = datetime.now().strftime("%Y%m%d_%H%M%S") + "_resumed"
LOG_DIR = f"runs/{RUN_ID}"
os.makedirs(LOG_DIR, exist_ok=True)

CSV_FILE     = f"{LOG_DIR}/all_evaluations.csv"
GEN_LOG_FILE = f"{LOG_DIR}/generations.log"
PARETO_FILE  = f"{LOG_DIR}/pareto_front.csv"

# Copy cache into new CSV so the new run has full history
with open(CSV_FILE, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(
        ["generation", "individual_id"] +
        [p["name"] for p in PARAMS] +
        [o["name"] for o in OBJECTIVES]
    )
    for i, (params, objectives) in enumerate(cache.items()):
        writer.writerow(["prior", i] + list(params) + list(objectives))

eval_counter = [loaded]   # continue numbering from where we left off

def log_evaluation(generation, params, objectives):
    eval_counter[0] += 1
    with open(CSV_FILE, "a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([generation, eval_counter[0]] + list(params) + list(objectives))

def log_generation(gen, fits):
    line = f"Gen {gen:3d} | " + " | ".join(
        f"{o['name']} best: {min(f[i] for f in fits) if o['minimize'] else max(f[i] for f in fits):.4f}"
        f"  avg: {sum(f[i] for f in fits) / len(fits):.4f}"
        for i, o in enumerate(OBJECTIVES)
    )
    print(line)
    with open(GEN_LOG_FILE, "a") as f:
        f.write(line + "\n")

# ================================================================
# STEP 4: Seed initial population from best cached results
#         instead of starting random
# ================================================================
POPULATION  = 100
GENERATIONS = 80   # will run full generations, but most evals hit cache

def _fitness(individual):
    values = simulate(list(individual))
    return tuple(values)

def run():
    print("Step 1: Building weights...", flush=True)
    weights = tuple(-1.0 if o["minimize"] else 1.0 for o in OBJECTIVES)

    print("Step 2: Creating DEAP types...", flush=True)
    if hasattr(creator, "Fitness"):    del creator.Fitness
    if hasattr(creator, "Individual"): del creator.Individual
    creator.create("Fitness",    base.Fitness, weights=weights)
    creator.create("Individual", list,         fitness=creator.Fitness)

    print("Step 3: Setting up toolbox...", flush=True)
    toolbox = base.Toolbox()
    for i, p in enumerate(PARAMS):
        toolbox.register(f"attr_{i}", random.randint, p["min"], p["max"])
    attrs = [getattr(toolbox, f"attr_{i}") for i in range(len(PARAMS))]
    toolbox.register("individual", tools.initCycle, creator.Individual, attrs, n=1)
    toolbox.register("population", tools.initRepeat, list, toolbox.individual)
    toolbox.register("evaluate", _fitness)
    toolbox.register("mate",     tools.cxUniform, indpb=0.5)
    toolbox.register("mutate",   tools.mutUniformInt,
                                 low  =[p["min"] for p in PARAMS],
                                 up   =[p["max"] for p in PARAMS],
                                 indpb=0.2)
    toolbox.register("select", tools.selNSGA2)

    print("Step 4: Seeding population from cache...", flush=True)
    sorted_cache = sorted(cache.items(), key=lambda x: x[1][0])
    seeded = []
    for params, objectives in sorted_cache[:POPULATION]:
        ind = creator.Individual(list(params))
        ind.fitness.values = objectives
        seeded.append(ind)
    print(f"  Seeded {len(seeded)} individuals from cache", flush=True)

    print("Step 5: Creating random individuals for remainder...", flush=True)
    remainder = max(0, POPULATION - len(seeded))
    print(f"  Need {remainder} random individuals", flush=True)
    pop = toolbox.population(n=remainder)
    print(f"  Created {len(pop)} random individuals", flush=True)

    print("Step 6: Evaluating random individuals...", flush=True)
    for i, ind in enumerate(pop):
        print(f"  Evaluating random individual {i+1}/{len(pop)}: {list(ind)}", flush=True)
        ind.fitness.values = toolbox.evaluate(ind)
        print(f"  Done: {ind.fitness.values}", flush=True)

    print("Step 7: Combining seeded + random population...", flush=True)
    pop = seeded + pop
    print(f"  Total population: {len(pop)}", flush=True)

    print("Step 8: Running NSGA2 selection on initial population...", flush=True)
    pop = toolbox.select(pop, POPULATION)
    print("  Done.", flush=True)

    fits = [ind.fitness.values for ind in pop]
    log_generation(0, fits)
    print(f"  (cache hits: {cache_counter[0]}, simulator calls: {call_counter[0]})", flush=True)


    for gen in range(1, GENERATIONS + 1):
        current_gen[0] = gen

        offspring = tools.selTournamentDCD(pop, len(pop))
        offspring = list(map(toolbox.clone, offspring))

        for ind1, ind2 in zip(offspring[::2], offspring[1::2]):
            if random.random() < 0.7:
                toolbox.mate(ind1, ind2)
                del ind1.fitness.values
                del ind2.fitness.values

        for ind in offspring:
            if random.random() < 0.2:
                toolbox.mutate(ind)
                del ind.fitness.values

        invalid = [ind for ind in offspring if not ind.fitness.valid]
        fitnesses = map(toolbox.evaluate, invalid)
        for ind, fit in zip(invalid, fitnesses):
            ind.fitness.values = fit

        pop = toolbox.select(pop + offspring, POPULATION)

        fits = [ind.fitness.values for ind in pop]
        log_generation(gen, fits)
        print(f"  (cache hits: {cache_counter[0]}, new simulator calls: {call_counter[0]})")

    # ── Pareto front ──────────────────────────────────────────
    pareto_front = tools.sortNondominated(pop, len(pop), first_front_only=True)[0]

    with open(PARETO_FILE, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([p["name"] for p in PARAMS] + [o["name"] for o in OBJECTIVES])
        for ind in pareto_front:
            writer.writerow(list(ind) + list(ind.fitness.values))

    print(f"\n=== DONE — {len(pareto_front)} Pareto-optimal solutions ===")
    print(f"Total cache hits  : {cache_counter[0]}")
    print(f"New simulator runs: {call_counter[0]}")
    print(f"Logs saved to     : {LOG_DIR}/")

    return pareto_front

if __name__ == "__main__":
    pareto = run()