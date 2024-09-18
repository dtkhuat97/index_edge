from subprocess import call
import csv
import os

#compressed_name = "dnb_set=authorities_partition=werk_compressed"


#date = "23_04_24"
#date = "03_06_2024"
date = "22_07_2024"
num_node_labels = 1000
num_edge_labels = 4
num_edges = 1000
str_len = 4

while str_len <= 64:

    path_og = "/home/dtkhuat/results/random_graphs/var_edge_len/"+ str(str_len) + "/"
    path_incidence =  "/home/dtkhuat/results/random_graphs_compressed/IncidenceRePair_compressed/var_edge_len/"+str(str_len)+"/"
    path_index = "/home/dtkhuat/results/random_graphs_compressed/IndexedEdges_compressed/var_edge_len/"+str(str_len)+"/"
    call(["mkdir",path_incidence])
    call(["mkdir",path_index])
    call(["touch",path_og+date+".csv"])

    
    
    
    for i in range(100):
        file_name = "random_"+str(num_node_labels)+"_4_"+str(num_edges)+"_"
        compressed_name = file_name + str(i) + "_c"
        #call(["/home/dtkhuat/IncidenceTypeRePair/build/./cgraph-cli", "--overwrite", "--rrr", "--verbose", path_og + file_name + str(i)+".ttl",  path_incidence+compressed_name])
        #call(["./cgraph-cli", "--overwrite", "--rrr", "--verbose",path_og + file_name + str(i)+".ttl", path_index + compressed_name])
        call(["/home/dtkhuat/IncidenceTypeRePair/build/./cgraph-cli", "--overwrite", "--rrr", "--verbose","--factor", "8", "--sampling", "32", "--max-rank","12", path_og + file_name + str(i)+".ttl",  path_incidence+compressed_name])
        call(["./cgraph-cli", "--overwrite", "--rrr", "--verbose", "--factor", "8", "--sampling", "32", "--max-rank","12" ,path_og + file_name + str(i)+".ttl", path_index + compressed_name])

        
        with open(path_og+date+'.csv', 'a') as csv_file:
            fields = [file_name+ str(i)+".ttl", os.stat(path_og + file_name+ str(i)+".ttl").st_size, os.stat(path_incidence+compressed_name).st_size, os.stat(path_index+compressed_name).st_size]
            csv_writer = csv.writer(csv_file, delimiter=',')
            csv_writer.writerow(fields)
        
        if(i<99):
            with open('/home/dtkhuat/results/random_graphs_compressed/IndexedEdges_compressed/var_edge_len/timings.csv', 'a') as csv_file:
               #csv_writer = csv.writer(csv_file, delimiter=',')
                csv_file.write(", ")
                csv_file.close()
            with open('/home/dtkhuat/results/random_graphs_compressed/IncidenceRePair_compressed/var_edge_len/timings.csv', 'a') as csv_file:
                #csv_writer = csv.writer(csv_file, delimiter=',')
                csv_file.write(", ")
                csv_file.close()
        else:
            with open('/home/dtkhuat/results/random_graphs_compressed/IndexedEdges_compressed/var_edge_len/timings.csv', 'a') as csv_file:
                csv_file.write("\n")
                csv_file.close()
            with open('/home/dtkhuat/results/random_graphs_compressed/IncidenceRePair_compressed/var_edge_len/timings.csv', 'a') as csv_file:
                csv_file.write("\n")
                csv_file.close()

    str_len = str_len + 4