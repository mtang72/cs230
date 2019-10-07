import re

with open("hashq_correctness.txt", "r") as f:
	ser_txt, par_txt = f.read().split("\n-------parallel-------\n")
ser_txt = list(filter(lambda x:not re.match(r"\s*$",x), ser_txt.split("\n-------end session-------\n")))
par_txt = list(filter(lambda x:not re.match(r"\s*$",x), par_txt.split("\n-------end session-------\n")))
ser_tots = list(map(lambda x:int(re.search(r"(?<=\nChecksum Total: )[0-9]+",x).group()), ser_txt))
f = open("parnoload_correctness_ver.txt","w+")

for s in range(len(par_txt)):
	queued = int(re.search(r"(?<=dispatched: )[0-9]+",par_txt[s]).group())
	processed = int(re.search(r"(?<=processed: )[0-9]+",par_txt[s]).group())
	if queued != processed:
		print("Processed packets in session {} unequal to dispatched packets: dispatched {}, processed {}".format(s+1,queued,processed),file=f)
	sumsum = int(re.search(r"(?<=\nChecksum Total: )[0-9]+",par_txt[s]).group())
	if sumsum != ser_tots[s]:
		print("Chksum total in exponential session {} unequal to serial chksum total: session total {}, real total {}".format(s+1,sumsum,ser_tots[s]),file=f)
	print(queued,processed,sumsum,ser_tots[s])

f.seek(0)
if f.read().strip().strip("\n") == "":
	print("Everything's all right!",file=f)
f.close()
