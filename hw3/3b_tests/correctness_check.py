import re

with open("actual_correctness.txt", "r") as f:
	uni_txt, exp_txt = f.read().split("\n-------exponential-------\n")
uni_txt = list(filter(lambda x:not re.match(r"\s*$",x), uni_txt.split("\n-------end session-------\n")))
exp_txt = list(filter(lambda x:not re.match(r"\s*$",x), exp_txt.split("\n-------end session-------\n")))
unitot = int(re.search(r"(?<=Checksum Total: )[0-9]+",uni_txt[0]).group())
exptot = int(re.search(r"(?<=Checksum Total: )[0-9]+",exp_txt[0]).group())
f = open("correctness_ver.txt","w+")
for s in range(1,len(uni_txt)):
	queued = int(re.search(r"(?<=dispatched: )[0-9]+",uni_txt[s]).group())
	processed = int(re.search(r"(?<=processed: )[0-9]+",uni_txt[s]).group())
	if queued != processed:
		print("Processed packets in session {} unequal to dispatched packets: dispatched {}, processed {}".format(s+1,queued,processed),file=f)
	sumsum = int(re.search(r"(?<=Checksum Total: )[0-9]+",uni_txt[s]).group())
	if sumsum != unitot:
		print("Chksum total in uniform session {} unequal to serial chksum total: session total {}, real total {}".format(s+1,sumsum,unitot),file=f)
	print(queued,processed,sumsum,unitot)

for s in range(1,len(exp_txt)):
	queued = int(re.search(r"(?<=dispatched: )[0-9]+",exp_txt[s]).group())
	processed = int(re.search(r"(?<=processed: )[0-9]+",exp_txt[s]).group())
	if queued != processed:
		print("Processed packets in session {} unequal to dispatched packets: dispatched {}, processed {}".format(s+1,queued,processed),file=f)
	sumsum = int(re.search(r"(?<=Checksum Total: )[0-9]+",exp_txt[s]).group())
	if sumsum != exptot:
		print("Chksum total in exponential session {} unequal to serial chksum total: session total {}, real total {}".format(s+1,sumsum,exptot),file=f)
	print(queued,processed,sumsum,exptot)

f.seek(0)
if f.read().strip().strip("\n") == "":
	print("Everything's all right!",file=f)
f.close()
