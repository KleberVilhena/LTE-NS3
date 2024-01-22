import sqlite3
import pandas as pd
import numpy as np
import glob
import subprocess
from pathlib import Path

def calc_distances(point, positions):
	#numpy.linalg.norm(a - b) is equivalent to the euclidean distance
	#between the vectors a and b
	sub = point - positions
	distances = sub.apply(np.linalg.norm, axis=1)
	ncols = len(distances)
	distances.index = [f'distance_{x}' for x in range(1,ncols+1)]
	return distances

ue_query = '''SELECT nodeapploss.nodeid,loss,cellid,x,y,nodeapploss.simulationtime
		FROM nodeapploss
		INNER JOIN lteuecell
		ON nodeapploss.nodeid = lteuecell.nodeid
		AND nodeapploss.simulationtime = lteuecell.simulationtime
		INNER JOIN nodelocation
		ON lteuecell.nodeid = nodelocation.nodeid
		AND lteuecell.simulationtime = nodelocation.simulationtime;'''

enb_query = '''SELECT nodelocation.nodeid,x,y
		FROM nodelocation
		INNER JOIN lteenb
		ON nodelocation.nodeid = lteenb.nodeid
		AND nodelocation.simulationtime = 2000000000'''

result = Path("./oran-repository-config0.db")

if not result.exists():
	for i in range(27):
		subprocess.run(f'''./ns3 run oran-lte-2-lte-ml-handover-train -- \
		--use-oran=true \
		--use-distance-lm=false \
		--use-onnx-lm=false \
		--use-torch-lm=false \
		--start-config={i} \
		--db-file=oran-repository-config{i}.db \
		--traffic-trace-file=traffic-trace-config{i}.tr \
		--position-trace-file=position-trace-config{i}.tr \
		--handover-trace-file=handover-trace-config{i}.tr \
		--sim-time=50''', shell=True)

files = sorted(glob.glob("./oran-repository-config*.db"))

data = []
for f in files:
	con = sqlite3.connect(f)
	ue_data = pd.read_sql_query(ue_query, con)

	mean_loss = ue_data.groupby('simulationtime', as_index=False)['loss'].mean()
	mean_loss = mean_loss.rename(columns={'loss': 'mean_loss'})
	ue_data = pd.merge(ue_data, mean_loss, how='right', on='simulationtime')

	data.append(ue_data)
data = pd.concat(data)

con = sqlite3.connect(files[0])
enb_data = pd.read_sql_query(enb_query, con)
distances = data[['x','y']].apply(calc_distances,
									axis='columns',
									result_type='expand',
									positions=enb_data[['x','y']])
serving_cell_distance = distances.values[
						np.arange(len(distances)),data['cellid']-1
						]
data = pd.concat([data, distances], axis='columns')
data['cell_dist'] = serving_cell_distance

data_grouped = data.groupby(['simulationtime', 'nodeid'])
optimal = []
for name, group in data_grouped:
	sort = group.sort_values(by=['mean_loss', 'cell_dist'])
	optimal.append(sort.iloc[0])
optimal = pd.DataFrame(optimal)

cell_mean = optimal.groupby(['simulationtime', 'cellid'])['loss'].mean()
cell_mean = cell_mean.unstack()
columns = cell_mean.columns.astype(int)
cell_mean.columns = [f'cell_mean_{x}' for x in columns]
cell_mean = cell_mean.reset_index()
optimal = pd.merge(optimal, cell_mean, how='right', on='simulationtime')

distances = optimal.filter(like='distance', axis='columns')
cell_mean = optimal.filter(like='cell_mean', axis='columns')
train_data = pd.concat([distances,
						cell_mean,
						optimal['loss'],
						optimal['cellid'].astype(int)-1,
						optimal['nodeid']],
						axis='columns')
train_data = train_data.loc[train_data['nodeid'] <= 3, :'cellid']
print(train_data)
train_data.to_csv('training.data', sep=' ', header=False, index=False)
