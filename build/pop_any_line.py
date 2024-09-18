import csv
with open('/home/dtkhuat/results/times/26_04_24.csv', 'r+') as inp, open('/home/dtkhuat/results/times/27_04_24.csv', 'w+') as out:
    writer = csv.writer(out)
    for row in csv.reader(inp):
        if row[1] != "":
            writer.writerow(row)