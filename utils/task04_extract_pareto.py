import csv
import os
import sys
from deap import base, creator, tools

def extract_pareto():
    # ================================================================
    # DYNAMIC INPUT
    # ================================================================
    print("--- Pareto Front Extractor ---")
    log_folder = input("Enter the path to your run folder (e.g., runs/20260421_095200): ").strip()

    if not os.path.exists(log_folder):
        print(f"Error: Folder '{log_folder}' not found.")
        return

    input_csv = os.path.join(log_folder, "all_evaluations.csv")
    output_csv = os.path.join(log_folder, "extracted_pareto_front.csv")

    if not os.path.exists(input_csv):
        print(f"Error: Could not find 'all_evaluations.csv' in {log_folder}")
        return

    # ================================================================
    # CONFIGURATION (Matches your predictor objectives)
    # ================================================================
    PARAM_NAMES = ["LOGLB", "NUMG", "LOGG", "LOGB", "TAGW", "GHIST", "LOGP1", "GHIST1"]
    OBJ_NAMES   = ["mispredictions", "p2 latency", "energy per instruction"]

    # 1. Setup DEAP for 3-objective minimization
    if hasattr(creator, "Fitness"): del creator.Fitness
    if hasattr(creator, "Individual"): del creator.Individual
    
    creator.create("Fitness", base.Fitness, weights=(-1.0, -1.0, -1.0))
    creator.create("Individual", list, fitness=creator.Fitness)

    all_individuals = []
    seen_params = set()

    # 2. Load data from the CSV
    print(f"Reading {input_csv}...")
    with open(input_csv, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                # Map CSV columns to tuples
                params = tuple(int(row[p]) for p in PARAM_NAMES)
                objs = tuple(float(row[o]) for o in OBJ_NAMES)

                # Ignore duplicates or failed simulations
                if params in seen_params or any(v == float('inf') for v in objs):
                    continue
                
                ind = creator.Individual(params)
                ind.fitness.values = objs
                all_individuals.append(ind)
                seen_params.add(params)
                
            except (ValueError, KeyError):
                continue

    if not all_individuals:
        print("No valid data found in the CSV.")
        return

    print(f"Loaded {len(all_individuals)} unique configurations.")

    # 3. Calculate Pareto Optimal set
    pareto_front = tools.sortNondominated(all_individuals, len(all_individuals), first_front_only=True)[0]

    # 4. Save results
    print(f"Found {len(pareto_front)} optimal trade-offs. Saving to {output_csv}...")
    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(PARAM_NAMES + OBJ_NAMES)
        for ind in pareto_front:
            writer.writerow(list(ind) + list(ind.fitness.values))

    print("\nExtraction complete. You can now analyze 'extracted_pareto_front.csv'.")

if __name__ == "__main__":
    extract_pareto()