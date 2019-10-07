import re
from sys import argv
#to check that checksums match serial htable version
with open("serhash_correctness.txt", "r") as f:
	ser_txt = list(filter(lambda x:not re.match(r"\s*$",x), f.read().split("\n-------end session-------\n")))

with open(argv[1],"r") as f:
	par_txt = list(filter(lambda x:not re.match(r"\s*$",x), f.read().split("\n-------end session-------\n")))

ser_tots = list(map(lambda x:int(re.search(r"(?<=\nChecksum Total: )[0-9]+",x).group()), ser_txt))
ser_adds = list(map(lambda x:int(re.search(r"(?<=\nAdd Checksum Total: )[0-9]+",x).group()), ser_txt))
#ser_size = list(map(lambda x:int(re.search(r"(?<=\nFinal Table Size: )[0-9]+",x).group()), ser_txt))
f = open("ver_"+argv[1].split("_")[0]+"_correctness.txt","w+")

for s in range(len(par_txt)):
	queued = int(re.search(r"(?<=dispatched: )[0-9]+",par_txt[s]).group())
	processed = int(re.search(r"(?<=processed: )[0-9]+",par_txt[s]).group())
	if queued != processed:
		print("Processed packets in session {} unequal to dispatched packets: dispatched {}, processed {}".format(s+1,queued,processed),file=f)
	sumsum = int(re.search(r"(?<=\nChecksum Total: )[0-9]+",par_txt[s]).group())
	if sumsum != ser_tots[s]:
		print("Chksum total in parallel session {} unequal to serial chksum total: session total {}, real total {}".format(s+1,sumsum,ser_tots[s]),file=f)
	addsumsum = int(re.search(r"(?<=\nAdd Checksum Total: )[0-9]+",par_txt[s]).group())
	eqv = "Yes" in par_txt[s]
	if addsumsum != ser_adds[s]:
		print("Add Chksum total in parallel session {} unequal to serial chksum total: session total {}, real total {}".format(s+1,addsumsum,ser_adds[s]),file=f)
	if not eqv:
		print("Add != Rmv + Rem: session {}".format(s+1), file=f)
	#finalsize = int(re.search(r"(?<=\nFinal Table Size: )[0-9]+",par_txt[s]).group())
	#if finalsize != ser_size[s]:
	#	print("Final table size does not equal serial: session {}, serial size {}, parallel size {}".format(s+1,ser_size[s],finalsize), file=f)
	print(queued,processed,sumsum,ser_tots[s])

f.seek(0)
if f.read().strip().strip("\n") == "":
	print("Everything's all right!",file=f)
f.close()
