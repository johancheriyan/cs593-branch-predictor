import csv
import os
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from deap import base, creator, tools

def main():
    print("--- Pareto Analysis & Matplotlib Visualization ---")
    
    # 1. User Inputs
    folder = input("Enter run folder (e.g., runs/20260421_095200): ").strip()
    filename = input("Enter CSV filename (e.g., all_evaluations.csv): ").strip()
    path = os.path.join(folder, filename)

    if not os.path.exists(path):
        print(f"Error: {path} not found.")
        return

    # Configuration (Must match your TAGE experiment)
    PARAM_NAMES = ["LOGLB", "NUMG", "LOGG", "LOGB", "TAGW", "GHIST", "LOGP1", "GHIST1"]
    OBJ_NAMES   = ["mispredictions", "p2 latency", "energy per instruction"]

    # 2. Setup DEAP for Pareto filtering
    if hasattr(creator, "Fitness"): del creator.Fitness
    if hasattr(creator, "Individual"): del creator.Individual
    creator.create("Fitness", base.Fitness, weights=(-1.0, -1.0, -1.0))
    creator.create("Individual", list, fitness=creator.Fitness)

    all_inds = []
    seen = set()

    # 3. Load Data using standard CSV module
    print(f"Reading {path}...")
    with open(path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                params = tuple(int(row[p]) for p in PARAM_NAMES)
                objs = tuple(float(row[o]) for o in OBJ_NAMES)
                
                # Filter out duplicates and failed runs
                if params in seen or any(v == float('inf') for v in objs):
                    continue
                
                ind = creator.Individual(params)
                ind.fitness.values = objs
                all_inds.append(ind)
                seen.add(params)
            except (ValueError, KeyError):
                continue

    # 4. Extract Pareto Front
    pareto_inds = tools.sortNondominated(all_inds, len(all_inds), first_front_only=True)[0]
    
    # Separate objectives for plotting
    mispredicts = [ind.fitness.values[0] for ind in pareto_inds]
    latency = [ind.fitness.values[1] for ind in pareto_inds]
    energy = [ind.fitness.values[2] for ind in pareto_inds]

    print(f"Found {len(pareto_inds)} Pareto-optimal points.")

    # 5. Plotting with Matplotlib
    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111, projection='3d')

    # Create the scatter plot
    # c=energy sets the color scale to the energy objective
    sc = ax.scatter(mispredicts, latency, energy, c=energy, cmap='viridis', s=50)

    # Labeling the axes
    ax.set_xlabel('Mispredictions')
    ax.set_ylabel('P2 Latency (cycles)')
    ax.set_zlabel('Energy per Instruction (fJ)')
    plt.title(f'3D Pareto Front: {filename}')
    
    # Add a color bar
    # cbar = plt.colorbar(sc, pad=0.1)
    # cbar.set_label('Energy Intensity')

    # Save the plot for your report
    save_path = os.path.join(folder, f"pareto_plot_{filename.split('.')[0]}.png")
    plt.savefig(save_path, dpi=300)
    print(f"Plot saved to: {save_path}")
    
    plt.show()

if __name__ == "__main__":
    main()