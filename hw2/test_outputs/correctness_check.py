from re import search
f = open("serial_correctness.txt", "a+")
serial = False
d = {}
for i in range(10):
	d[i] = []
f.seek(0)
txt = f.read().split('\n')
for line in txt:
	if len(line)>8 and line[:3]!="enq":
		num = int(line[8])
		tup = (search(r'(?<=chksum: )[0-9]+',line).group(), -1)
		if tup not in d[num]:
			d[num].append(tup)
if not serial:
	for line in txt:
		if len(line)>8 and line[:3]=="enq":
			num = int(line[12])
			pck = search(r'(?<=chksum: )[0-9]+',line).group()
			for i in range(len(d[num])):
				if d[num][i][0] == pck:
					d[num][i] = (d[num][i][0], pck)
for i in d.keys():
	matchers = 0
	for pair in d[i]:
		if pair[0] == pair[1]:
			matchers+=1
	print("src: {} number of unique packets for this source: {} number of packets that match enq: {}".format(i,len(d[i]),matchers))
f.close()