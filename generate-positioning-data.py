import random as rd
import pandas as pd

X = [rd.randint(-790,790) for _ in range(2000)]
Y = [rd.randint(-620,620) for _ in range(2000)]

coords = {'x': X, 'y': Y}
coords = pd.DataFrame(coords)
coords.to_csv('train-initial-positions.tr',sep=' ', header=False, index=False)
