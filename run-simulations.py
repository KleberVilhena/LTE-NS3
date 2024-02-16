#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sem
import argparse
import shutil
from pathlib import Path

ns_path = './'
script = 'oran-lte-2-lte-ml-handover-simulation'
campaign_dir = ns_path + 'zram/sem'
persist_campaign_dir = ns_path + 'sem'
results_dir = ns_path + 'results'
nRuns = 32

parser = argparse.ArgumentParser(description='SEM script')
parser.add_argument('-e', '--export', action='store_true',
                    help="Don't run simulations. Just export the results.")
parser.add_argument('-o', '--overwrite', action='store_true',
                    help='Overwrite previous campaign.')
args = parser.parse_args()

campaign = sem.CampaignManager.new(ns_path, script, campaign_dir,
            overwrite=args.overwrite, check_repo=False)

param_combinations = {
	'traffic-trace-file': '/dev/null',
	'position-trace-file': '/dev/null',
	'handover-trace-file': '/dev/null',
    'sim-time': 200,
    'num-ues': [50, 75, 100],
	'handover-algorithm': 'ns3::A2A4RsrqHandoverAlgorithm',
	'use-torch-lm': [False]
}
if(not args.export):
    print("running simulations with traditional handover")
    campaign.run_missing_simulations(sem.list_param_combinations(param_combinations),
                    runs=nRuns, stop_on_errors=False)

result_param = { 
    'num-ues': [50, 75, 100],
	'handover-algorithm': ['ns3::A2A4RsrqHandoverAlgorithm'],
	'use-torch-lm': [False]
}
print("exporting results")
campaign.save_to_folders(result_param, results_dir + '/sem ia', nRuns)

param_combinations['handover-algorithm'] = 'ns3::NoOpHandoverAlgorithm'
param_combinations['use-torch-lm'] = True
if(not args.export):
    print("running simulations with IA based handover")
    campaign.run_missing_simulations(sem.list_param_combinations(param_combinations),
                    runs=nRuns, stop_on_errors=False)

result_param['handover-algorithm'] = ['ns3::NoOpHandoverAlgorithm']
result_param['use-torch-lm'] = [True]
print("exporting results")
campaign.save_to_folders(result_param, results_dir + '/com ia', nRuns)

print("saving campaign")
if Path(persist_campaign_dir).exists():
	shutil.rmtree(persist_campaign_dir)
shutil.copytree(campaign_dir, persist_campaign_dir)
