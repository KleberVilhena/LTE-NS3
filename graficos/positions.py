import sqlite3
import pandas as pd
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt

ue_query = '''SELECT nodeapploss.nodeid,x,y
		FROM nodeapploss
		INNER JOIN nodelocation
		ON nodeapploss.nodeid = nodelocation.nodeid
		AND nodeapploss.simulationtime = nodelocation.simulationtime
		WHERE nodeapploss.simulationtime = 2000000000'''

enb_query = '''SELECT nodelocation.nodeid,x,y
		FROM nodelocation
		INNER JOIN lteenb
		ON nodelocation.nodeid = lteenb.nodeid
		AND nodelocation.simulationtime = 2000000000'''

result = Path("../oran-repository.db")

con = sqlite3.connect(result)
ue_data = pd.read_sql_query(ue_query, con)
enb_data = pd.read_sql_query(enb_query, con)

plt.gca().set_aspect('equal')
plt.scatter(ue_data['x'], ue_data['y'], label='UEs')
plt.scatter(enb_data['x'], enb_data['y'], label='ENBs')
plt.legend()
plt.savefig("positions.png", format='png', dpi=300, bbox_inches = "tight")
