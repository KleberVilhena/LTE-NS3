import random as rd
import math
import pandas as pd

X = []
Y = []
xcenter = 0
ycenter = 0
rho = 100 #raio do disco
n_coords = 2000 #número de posições pra gerar

for _ in range(n_coords):
	x = rho
	y = rho

	while math.sqrt(x * x + y * y) > rho:
		x = rd.randint(-rho, rho)
		y = rd.randint(-rho, rho)

	X.append(x+xcenter)
	Y.append(y+ycenter)

coords = {'x': X, 'y': Y}
coords = pd.DataFrame(coords)
coords.to_csv('train-initial-positions.tr',sep=' ', header=False, index=False)
