import random as rd
import pandas as pd

n_coords = 2000 #número de posições pra gerar

X = [rd.randint(-100,100) for _ in range(n_coords)]
Y = [rd.randint(-100,100) for _ in range(n_coords)]

coords = {'x': X, 'y': Y}
coords = pd.DataFrame(coords)
coords.to_csv('train-initial-positions.tr',sep=' ', header=False, index=False)
