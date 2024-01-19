#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import math
import numpy as np
import pandas as pd
from matplotlib import pyplot as plt
import csv
import glob
from collections import namedtuple

def readFile(filePath):
	result = pd.read_csv(filePath, sep=',', header=0, usecols=['Time',
						'Delay', 'Jitter', 'Throughtput', 'PDR'])

	result = result.groupby('Time', as_index=False).mean(numeric_only=True)
	result['Delay'] = result['Delay'] * 1000
	result['Jitter'] = result['Jitter'] * 1000

	return result

RESULT_PATH = "../resultados"

ScenarioVariant = namedtuple("ScenarioVariant", ["name", "path", "color"])

scenario_variants = [ScenarioVariant("Sem IA",
				RESULT_PATH + "/..", "#ffc0cb"),
					 ScenarioVariant("IA Beta",
				RESULT_PATH + "/beta", "#ffc000"),
					 ScenarioVariant("IA RC1",
				RESULT_PATH + "/rc1", "#00c0c0"),
					 ScenarioVariant("IA RC2",
				RESULT_PATH + "/rc2", "#f4442d")]

fig_pdr = plt.figure()
ax_pdr = fig_pdr.subplots()

fig_delay = plt.figure()
ax_delay = fig_delay.subplots()

fig_jitter = plt.figure()
ax_jitter = fig_jitter.subplots()

fig_tp = plt.figure()
ax_tp = fig_tp.subplots()

width = 0.15       # the width of the bars
legend_cols = 4
scale = (len(scenario_variants)-1)/2

pdr_rectangles = []
delay_rectangles = []
jitter_rectangles = []
tp_rectangles = []

for variant in scenario_variants:
	data_scenarios = readFile(variant.path + "/qos-vs-time.txt")
	print(data_scenarios)

	mean = data_scenarios
	time = mean['Time'].unique()
	pdr = mean['PDR']
	rect = ax_pdr.plot(time, pdr, alpha=1.0, color=variant.color, linewidth=2.0)
	pdr_rectangles.append(rect[0])
	ax_pdr.set_xlabel("Time (s)", fontsize=17)
	ax_pdr.tick_params(axis="x", labelsize=15)
	#ax_pdr.set_yticks(np.arange(80,101,5))
	ax_pdr.set_ylabel("PDR (%)", fontsize=17)
	ax_pdr.tick_params(axis="y", labelsize=15)
	ax_pdr.yaxis.grid(color='gray', linestyle='dotted')

	delay = mean['Delay']
	rect = ax_delay.plot(time, delay, alpha=1.0, color=variant.color,
			linewidth=2.0)
	delay_rectangles.append(rect[0])
	ax_delay.set_xlabel("Time (s)", fontsize=17)
	ax_delay.tick_params(axis="x", labelsize=15)
	#ax_delay.set_yticks(np.arange(0,121,10))
	ax_delay.set_ylabel("Delay (ms)", fontsize=17)
	ax_delay.tick_params(axis="y", labelsize=15)
	ax_delay.yaxis.grid(color='gray', linestyle='dotted')

	jitter = mean['Jitter']
	rect = ax_jitter.plot(time, jitter, alpha=1.0, color=variant.color,
			linewidth=2.0)
	jitter_rectangles.append(rect[0])
	ax_jitter.set_xlabel("Time (s)", fontsize=17)
	ax_jitter.tick_params(axis="x", labelsize=15)
	#ax_jitter.set_yticks(np.arange(0,121,10))
	ax_jitter.set_ylabel("Jitter (ms)", fontsize=17)
	ax_jitter.tick_params(axis="y", labelsize=15)
	ax_jitter.yaxis.grid(color='gray', linestyle='dotted')

	tp = mean['Throughtput']
	rect = ax_tp.plot(time, tp, alpha=1.0, color=variant.color,
			linewidth=2.0)
	tp_rectangles.append(rect[0])
	ax_tp.set_xlabel("Time (s)", fontsize=17)
	ax_tp.tick_params(axis="x", labelsize=15)
	#ax_tp.set_yticks(np.arange(0,101,10))
	ax_tp.set_ylabel("Throughput", fontsize=17)
	ax_tp.tick_params(axis="y", labelsize=15)
	ax_tp.yaxis.grid(color='gray', linestyle='dotted')

labels = [variant.name for variant in scenario_variants]
ax_pdr.legend(pdr_rectangles, labels, loc='upper center', bbox_to_anchor=(0., 1.04, 0.95, .102),
	    ncol=legend_cols, borderaxespad=0.,  prop={'size': 14})
ax_delay.legend(delay_rectangles, labels, loc='upper center', bbox_to_anchor=(0., 1.04, 0.95, .102),
	    ncol=legend_cols, borderaxespad=0.,  prop={'size': 14})
ax_jitter.legend(jitter_rectangles, labels, loc='upper center', bbox_to_anchor=(0., 1.04, 0.95, .102),
	    ncol=legend_cols, borderaxespad=0.,  prop={'size': 14})
ax_tp.legend(tp_rectangles, labels, loc='upper center', bbox_to_anchor=(0., 1.04, 0.95, .102),
	    ncol=legend_cols, borderaxespad=0.,  prop={'size': 14})

fig_pdr.savefig("pdr-vs-time.png", format='png', dpi=300, bbox_inches = "tight")
fig_pdr.savefig("pdr-vs-time.pdf", format='pdf', dpi=300, bbox_inches = "tight")
fig_delay.savefig("delay-vs-time.png", format='png', dpi=300, bbox_inches = "tight")
fig_delay.savefig("delay-vs-time.pdf", format='pdf', dpi=300, bbox_inches = "tight")
fig_jitter.savefig("jitter-vs-time.png", format='png', dpi=300, bbox_inches = "tight")
fig_jitter.savefig("jitter-vs-time.pdf", format='pdf', dpi=300, bbox_inches = "tight")
fig_tp.savefig("tp-vs-time.png", format='png', dpi=300, bbox_inches = "tight")
fig_tp.savefig("tp-vs-time.pdf", format='pdf', dpi=300, bbox_inches = "tight")
#plt.show()
