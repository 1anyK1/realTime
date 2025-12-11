import matplotlib.pyplot as plt
import sys

def main():

    if len(sys.argv) != 2:
        print(f"Programm expected one argumet, but received {len(sys.argv)}")
        sys.exit(1)

    filename = sys.argv[1]
    tokens = []

    #open file
    with open(filename, "r") as f:
        for line in f:
            #split string on tokens
            tokens.append(line.split())

    #get list of iters and latency
    iters = [int(x[0]) for x in tokens if x[0].isdigit()]
    latency = [int(x[1]) for x in tokens if x[0].isdigit()]

    plt.plot(iters, latency)
    plt.xlabel("iterations")
    plt.ylabel("latency")
    plt.title(f"{sys.argv[1]}")
    plt.show()

if __name__ == '__main__':
    main()