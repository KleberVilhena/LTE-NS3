#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sqlite3
import pandas as pd
import numpy as np
import glob
import sem
import argparse
import shutil
from pathlib import Path

ns_path = './'
script = 'oran-lte-2-lte-ml-handover-train'
campaign_dir = ns_path + 'sem-train'
results_dir = ns_path + 'results-train'

def getScenarioParameters(directory):
	parameters = dict()
	subdir = directory.split('/')
	for s in subdir:
		if s.find('=') != -1:
			p = s.split('=')
			if p[1].isnumeric():
				parameters[p[0]] = int(p[1])
			else:
				parameters[p[0]] = p[1]
	parameters["path"] = directory
	return parameters

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

parser = argparse.ArgumentParser(description='dataset generation script')
parser.add_argument('-o', '--overwrite', action='store_true',
                    help='Overwrite previous campaign.')
args = parser.parse_args()

campaign = sem.CampaignManager.new(ns_path, script, campaign_dir,
            overwrite=args.overwrite, check_repo=False)
if args.overwrite and Path(results_dir).exists():
	shutil.rmtree(results_dir)

param_combinations = {
		'use-oran': True,
		'use-distance-lm': False,
		'use-onnx-lm': False,
		'use-torch-lm': False,
		'scenario': list(range(3)),
		'start-config': list(range(3)),
		'run-id': list(range(100)),
		'sim-time': 10,
		'RngRun': 0
		}

campaign.run_missing_simulations(sem.list_param_combinations(param_combinations),
                    stop_on_errors=False)

result_param = {
		'scenario': list(range(3)),
		'start-config': list(range(3)),
		'run-id': list(range(100))
		}
if not Path(results_dir).exists():
	campaign.save_to_folders(result_param, results_dir, 1)

files = glob.glob(results_dir + 
				  "/scenario=*/start-config=*/run-id=*/run=0/oran-repository.db")
data = []
for f in files:
	parameters = getScenarioParameters(f)
	con = sqlite3.connect(f)
	ue_data = pd.read_sql_query(ue_query, con)

	time = ue_data[ue_data['nodeid'] == 1]['simulationtime']
	start = time.iloc[0]
	end = time.iloc[-1]
	step = time.iloc[1] - start

	next_loss = ue_data[ue_data['simulationtime'] > start]['loss'].reset_index(drop=True)
	ue_data = ue_data[ue_data['simulationtime'] < end].reset_index(drop=True)
	ue_data['next-loss'] = next_loss
	ue_data['scenario'] = parameters['scenario']
	ue_data['run-id'] = parameters['run-id']
	ue_data['start-config'] = parameters['start-config']

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

data_grouped = data.groupby(['scenario', 'run-id', 'simulationtime'])
optimal = []
for name, group in data_grouped:
	sort = group[group['nodeid'] == 1].sort_values(by=['next-loss', 'cell_dist'])
	best = sort.iloc[0]
	sel = group[group['start-config'] != best['start-config']].copy()
	#keep current cells
	sel['target_cell'] = sel['cellid']
	#only change cell for nodeid 1
	sel.loc[sel['nodeid'] == 1, 'target_cell'] = best['cellid']
	optimal.append(sel)
optimal = pd.concat(optimal)

distances = optimal.loc[:, 'distance_1':'distance_3']
target_cell_distance = distances.values[
						np.arange(len(distances)),optimal['target_cell']-1
						]
optimal['target_cell_dist'] = target_cell_distance

cell_mean = optimal.groupby(['scenario', 'run-id', 'start-config',
							 'simulationtime', 'cellid'])['loss'].mean()
cell_mean = cell_mean.unstack()
columns = cell_mean.columns.astype(int)
cell_mean.columns = [f'cell_mean_{x}' for x in columns]
cell_mean = cell_mean.reset_index()
optimal = pd.merge(optimal, cell_mean, how='right',
				   on=['scenario', 'run-id', 'start-config', 'simulationtime'])
optimal = optimal[optimal['nodeid'] == 1].reset_index()

#reorder enbs based on distance
sel = optimal.loc[:, 'distance_1':]
sorted_rows = []
for row in sel.itertuples(index=False):
	target_dist = row[5]
	d = {'distances': row[:3],
		 'losses': row[6:]}
	enbs = pd.DataFrame(d)
	enbs = enbs.sort_values(by='distances')
	enbs = enbs.reset_index(drop=True)
	index = enbs[enbs['distances'] == target_dist].index[0]
	enbs = enbs.unstack()
	labels = list(row._fields)
	del labels[3:6]
	enbs.index = labels
	enbs['target_cell'] = index
	sorted_rows.append(enbs)
train_data = pd.DataFrame(sorted_rows)

train_data['target_cell'] = train_data['target_cell'].astype(int)
train_data.insert(6, 'loss', optimal['loss'])
print(train_data)
train_data.to_csv('training-no-norm.data', sep=' ', header=False, index=False)

train_data.iloc[:,:2] = train_data.iloc[:,:2].div(train_data['distance_3'],
												  axis='index')
del train_data['distance_3']
print(train_data)
print(pd.cut(train_data['loss'], bins=20).value_counts(sort=False)/len(train_data))
train_data.to_csv('training.data', sep=' ', header=False, index=False)
