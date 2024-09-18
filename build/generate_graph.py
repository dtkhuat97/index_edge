#AVAILABLE CHARS
import random
import sys
from subprocess import call
avail_chars = ['0123456789abcdefghijklmnopqrstuvwxyz']
#Für jeden Knoten die Länge des Strings random ziehen und dann jeden einzelnen char ziehen
compressed_file = "random"
num_node_labels = int(sys.argv[1])
num_edge_labels = int(sys.argv[2])
num_edges = int(sys.argv[3])
str_len = 4


while str_len <= 64:
    call(["mkdir","/home/dtkhuat/results/random_graphs/var_edge_len/"+str(str_len)])
    call(["mkdir","/home/dtkhuat/results/random_nodes/var_edge_len/"+str(str_len)])
    call(["mkdir","/home/dtkhuat/results/random_edges/var_edge_len/"+str(str_len)])
    for i in range(100):
        edges = []
        node_labels = []
        edge_labels = []
        while len(node_labels) < num_node_labels:
            node_length = random.randrange(1, str_len)
            node_string = "<"
            for text_char in range(node_length):
                node_string = node_string + (avail_chars[0][random.randrange(0, len(avail_chars[0]))])
            node_string = node_string + ">"
            if(node_string not in node_labels):
                node_labels.append(node_string)
            
        #print(node_labels)

        while len(edge_labels) < num_edge_labels:
            label_length = random.randrange(1, str_len)
            label_string = "<"
            for text_char in range(label_length):
                label_string = label_string + (avail_chars[0][random.randrange(0, len(avail_chars[0]))])
            label_string = label_string + ">"
            if(label_string not in edge_labels):
                edge_labels.append(label_string)
        #print(labels)

        #WRITE random combination of <NODE><EDGE><NODE> to .nt file



        while len(edges)<num_edges:
            edge = node_labels[random.randrange(0, num_node_labels)] + " " + edge_labels[random.randrange(0, len(edge_labels))] + " " + node_labels[random.randrange(0, num_node_labels)] + " ." +"\n"
            if edge not in edges:
                edges.append(edge)
        #print(edges)


        path1 = "/home/dtkhuat/results/random_graphs/var_edge_len/"+ str(str_len) + "/" + compressed_file+ "_" + str(num_node_labels) + "_" + str(num_edge_labels) + "_" + str(num_edges) + "_" +str(i) + ".ttl"
        f = open(path1, "w")
        for edge in range(num_edges):
            f.write(edges[edge])
        f.close()

        path_nodes = "/home/dtkhuat/results/random_nodes/var_edge_len/"+ str(str_len) + "/" + compressed_file+ "_"  + str(num_node_labels) + "_" + str(num_edge_labels) + "_" + str(num_edges) + "_" +str(i) +".txt"
        path_edges = "/home/dtkhuat/results/random_edges/var_edge_len/"+ str(str_len) + "/" + compressed_file+ "_" + str(num_node_labels) + "_" + str(num_edge_labels) + "_" + str(num_edges) + "_" +str(i) +".txt"
        f = open(path_nodes, "w")
        for el in node_labels:
            f.write(el + "\n")
        f.close()
        f = open(path_edges, "w")
        for el in edge_labels:
            f.write(el + "\n")
        f.close()
        print(str(str_len) + "," + str(i))
    str_len = str_len + 4