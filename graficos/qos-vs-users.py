#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import math
import numpy as np
import pandas as pd
from matplotlib import pyplot as plt
import csv
import glob
from collections import namedtuple

def getLastNum(directory):
	subdir = directory.split('/')[-1]
	num = int(subdir.split('=')[-1])
	return num

def getScenarios(path):
	scenarios = dict()

	directories = glob.glob(path + "/num-ues=*")
	directories.sort(key=getLastNum)
	print(directories)
	for directory in directories:
		numUEs = getLastNum(directory)
		scenarios[numUEs] = directory

	return scenarios


def readFile(filePath):
	result = pd.read_csv(filePath, sep=',', header=0, usecols=['Time',
						'Delay', 'Jitter', 'Throughput', 'PDR'])

	grouped = result.groupby('Time', as_index=False)
	tp = grouped['Throughput'].sum().mean()

	result = result.mean()
	result['Delay'] = result['Delay'] * 1000
	result['Jitter'] = result['Jitter'] * 1000
	result['Throughput'] = tp['Throughput']

	return result

def getData(scenarios):
	data = []
	for scenario in scenarios.items():
		runs = glob.glob(scenario[1] + "/run=*/qos-vs-time.txt")
		qos = []
		for run in runs:
			qos_run = readFile(run)
			qos.append(qos_run)

		if runs == []:
			print("scenario " + scenario[1] + " has no data")
			zeroes = pd.Series(data=[0, 0, 0, 0], index=['PDR', 'Throughput', 'Delay','Jitter'])
			d = {'mean' : zeroes, 'std' : zeroes}
		else:
			print("scenario {} has {} data points".format(scenario[1], len(runs)))
			qos = pd.DataFrame(qos)
			d = {'mean' : qos.mean(), 'std' : qos.std()}
		qos_result = pd.DataFrame(d)
		qos_result['num-ues'] = scenario[0]

		data.append(qos_result)
	data = pd.concat(data)
	print("")
	return data

ScenarioVariant = namedtuple("ScenarioVariant", ["name", "path", "color"])
RESULT_PATH = "../resultados"

scenario_variants = [ScenarioVariant("Traditional O-RAN",
				RESULT_PATH + "/use-torch-lm=False", "#ffc0cb"),
					 ScenarioVariant("O-RAN AI Based",
				RESULT_PATH + "/use-torch-lm=True", "#f4442d")]

fig_pdr = plt.figure()
ax_pdr = fig_pdr.subplots()

fig_tp = plt.figure()
ax_tp = fig_tp.subplots()

fig_delay = plt.figure()
ax_delay = fig_delay.subplots()

fig_jitter = plt.figure()
ax_jitter = fig_jitter.subplots()

ind = np.arange(3)
width = 0.15       # the width of the bars
legend_cols = 4
variantN = 0
scale = (len(scenario_variants)-1)/2

pdr_rectangles = []
tp_rectangles = []
delay_rectangles = []
jitter_rectangles = []

complete_data = []
for variant in scenario_variants:
	scenarios = getScenarios(variant.path)
	data_scenarios = getData(scenarios)

	pdr = data_scenarios.loc['PDR']
	pdr_mean = pdr['mean']
	pdr_std = pdr['std']
	rect = ax_pdr.bar(ind + variantN*width, pdr_mean-50, width, yerr=pdr_std, bottom=50, align='center', alpha=1.0, ecolor='black', capsize=6, color=variant.color, linewidth=1.0, edgecolor='black')
	pdr_rectangles.append(rect[0])
	ax_pdr.set(xticks=ind+width*scale, xticklabels=list(scenarios.keys()))
	ax_pdr.set_xlabel("Number of users", fontsize=17)
	ax_pdr.tick_params(axis="x", labelsize=15)
	ax_pdr.set_yticks(np.arange(50,101,10))
	ax_pdr.set_ylabel("PDR (%)", fontsize=17)
	ax_pdr.tick_params(axis="y", labelsize=15)
	ax_pdr.yaxis.grid(color='gray', linestyle='dotted')

	tp = data_scenarios.loc['Throughput']
	tp_mean = tp['mean']
	tp_std = tp['std']
	rect = ax_tp.bar(ind + variantN*width, tp_mean, width, yerr=tp_std, align='center', alpha=1.0, ecolor='black', capsize=6, color=variant.color, linewidth=1.0, edgecolor='black')
	tp_rectangles.append(rect[0])
	ax_tp.set(xticks=ind+width*scale, xticklabels=list(scenarios.keys()))
	ax_tp.set_xlabel("Number of users", fontsize=17)
	ax_tp.tick_params(axis="x", labelsize=15)
#	ax_tp.set_yticks(np.arange(50,101,10))
	ax_tp.set_ylabel("Throughput (Mb/s)", fontsize=17)
	ax_tp.tick_params(axis="y", labelsize=15)
	ax_tp.yaxis.grid(color='gray', linestyle='dotted')

	delay = data_scenarios.loc['Delay']
	delay_mean = delay['mean']
	delay_std = delay['std']
	rect = ax_delay.bar(ind + variantN*width, delay_mean, width, yerr=delay_std, align='center', alpha=1.0, ecolor='black', capsize=6, color=variant.color, linewidth=1.0, edgecolor='black')
	delay_rectangles.append(rect[0])
	ax_delay.set(xticks=ind+width*scale, xticklabels=list(scenarios.keys()))
	ax_delay.set_xlabel("Number of users", fontsize=17)
	ax_delay.tick_params(axis="x", labelsize=15)
	#ax_delay.set_yticks(np.arange(0,121,10))
	ax_delay.set_ylabel("Delay (ms)", fontsize=17)
	ax_delay.tick_params(axis="y", labelsize=15)
	ax_delay.yaxis.grid(color='gray', linestyle='dotted')

	jitter = data_scenarios.loc['Jitter']
	jitter_mean = jitter['mean']
	jitter_std = jitter['std']
	rect = ax_jitter.bar(ind + variantN*width, jitter_mean, width, yerr=jitter_std, align='center', alpha=1.0, ecolor='black', capsize=6, color=variant.color, linewidth=1.0, edgecolor='black')
	jitter_rectangles.append(rect[0])
	ax_jitter.set(xticks=ind+width*scale, xticklabels=list(scenarios.keys()))
	ax_jitter.set_xlabel("Number of users", fontsize=17)
	ax_jitter.tick_params(axis="x", labelsize=15)
	#ax_jitter.set_yticks(np.arange(0,26,5))
	ax_jitter.set_ylabel("Jitter (ms)", fontsize=17)
	ax_jitter.tick_params(axis="y", labelsize=15)
	ax_jitter.yaxis.grid(color='gray', linestyle='dotted')

	data_scenarios['handover'] = variant.name
	complete_data.append(data_scenarios)
	variantN += 1
complete_data = pd.concat(complete_data)
del complete_data['std']
complete_data = complete_data[complete_data.index != 'Time'].reset_index()
complete_data = complete_data.pivot(columns='index',
					index=['num-ues', 'handover'],
					values='mean')
print(complete_data)

labels = [variant.name for variant in scenario_variants]
ax_pdr.legend(pdr_rectangles, labels, loc='upper center', bbox_to_anchor=(0., 1.04, 0.95, .102),
	    ncol=legend_cols, borderaxespad=0.,  prop={'size': 14})
ax_tp.legend(tp_rectangles, labels, loc='upper center', bbox_to_anchor=(0., 1.04, 0.95, .102),
	    ncol=legend_cols, borderaxespad=0.,  prop={'size': 14})
ax_delay.legend(delay_rectangles, labels, loc='upper center', bbox_to_anchor=(0., 1.04, 0.95, .102),
	    ncol=legend_cols, borderaxespad=0.,  prop={'size': 14})
ax_jitter.legend(jitter_rectangles, labels, loc='upper center', bbox_to_anchor=(0., 1.04, 0.95, .102),
	    ncol=legend_cols, borderaxespad=0.,  prop={'size': 14})

fig_pdr.savefig("pdr-vs-users.png", format='png', dpi=300, bbox_inches = "tight")
fig_pdr.savefig("pdr-vs-users.pdf", format='pdf', dpi=300, bbox_inches = "tight")
fig_tp.savefig("tp-vs-users.png", format='png', dpi=300, bbox_inches = "tight")
fig_tp.savefig("tp-vs-users.pdf", format='pdf', dpi=300, bbox_inches = "tight")
fig_delay.savefig("delay-vs-users.png", format='png', dpi=300, bbox_inches = "tight")
fig_delay.savefig("delay-vs-users.pdf", format='pdf', dpi=300, bbox_inches = "tight")
fig_jitter.savefig("jitter-vs-users.png", format='png', dpi=300, bbox_inches = "tight")
fig_jitter.savefig("jitter-vs-users.pdf", format='pdf', dpi=300, bbox_inches = "tight")
#plt.show()
