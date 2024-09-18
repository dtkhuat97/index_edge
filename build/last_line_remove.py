import csv
f = open('/home/dtkhuat/results/times/11_06_2024.csv', "r+")
lines = f.readlines()
lines.pop()
f = open('/home/dtkhuat/results/times/11_06_2024.csv', "w+")
f.writelines(lines)