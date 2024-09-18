from subprocess import call
import csv
import os
file_name = "ontology.ttl"
compressed_name = "ontology_compressed"
path_og = "/home/dtkhuat/eval_datasets/"
path_incidence =  "/home/dtkhuat/results/IncidenceRePair_compressed/"
path_index = "/home/dtkhuat/results/IndexedEdges_compressed/"
#date = "26_04_24"
date = "11_06_2024"
with open('/home/dtkhuat/results/times/'+date+'.csv', 'a') as csv_file:
    csv_file.write(file_name+", ")
    csv_file.close()
call(["/home/dtkhuat/IncidenceTypeRePair/build/./cgraph-cli", "--overwrite", "--rrr", "--verbose", path_og + file_name,  path_incidence+compressed_name])

with open('/home/dtkhuat/results/times/'+date+'.csv', 'a') as csv_file:
    csv_file.write(", ")
    csv_file.close()

call(["./cgraph-cli", "--overwrite", "--rrr", "--verbose",path_og+file_name, path_index + compressed_name])

with open('/home/dtkhuat/results/times/'+date+'.csv', 'a') as csv_file:
    csv_file.write("\n")
    csv_file.close()