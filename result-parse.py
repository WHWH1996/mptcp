import sys
import os
import subprocess
import threading

def get_files_name(folderName):
	return [_fileName for _fileName in os.listdir(folderName) if _fileName[-4:] == ".xml"]

#ps_flow_MLT_mlp_bw10_lossrt0.1_2to2_s5_ps2_worker3_o1_rto10.xml 0
#ring_flow_MLT_mlp_bw10_lossrt0.1_2to2_s5_worker3_o1_rto10.xml 0
def parse_trace_name(fileName):
	slice_info = fileName.split("_")
	return "-".join([slice_info[0].split("/")[1], slice_info[3], slice_info[8], slice_info[2]])

def get_res(fileName):
	cmd = "python flowmon-parse-results.py " + fileName
	process = subprocess.Popen(cmd, shell=True,
              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	result_f = process.stdout.read()
	return float(result_f.split("\n")[5].split(":")[1][1:-1]), float(result_f.split("\n")[13].split(":")[1][1:-1])

results = dict()
resultsLock = threading.Lock()

def parse_file(fileName):
	traceName = parse_trace_name(fileName)
	ICT, loss = get_res(fileName)
	if(traceName.split("-")[0] == "ring"):
		ICT = ICT*float(traceName.split("-")[2][6:])*2*9
	resultsLock.acquire()
	results[traceName] = [ICT, loss]
	resultsLock.release()

def main(argv):
	filesName = get_files_name(argv[1])
	parseThreads = list()
	for _fileName in filesName:
		_t = threading.Thread(target=parse_file, args=(argv[1]+"/"+_fileName,))
		parseThreads.append(_t)
		_t.start()

	for _t in parseThreads:
		_t.join()

	for key in sorted(results.keys()):
		print("%s %.4f %.2f" %(key, results[key][0], results[key][1]))

if __name__ == '__main__':
    #print("parse the folder: %s" %(sys.argv[1]))
    main(sys.argv)
