import re

with open("contains_correctness.txt", "r") as f:
	txt = list(filter(lambda x:not re.match(r"\s*$",x), f.read().split("\n-------end session-------\n")))

f = open("ver_contains.txt", "w+")

for i in range(len(txt)):
	table = set()
	acts = txt[i].strip("\n").split("\n")
	for j in range(4,len(acts)):
		act = acts[j]
		if re.match(r"Add",act):
			table.add(int(re.search(r"[0-9]+",act).group()))
		elif re.match(r"Remove",act):
			key = int(re.search(r"[0-9]+",act).group())
			if key not in table:
				if re.search(r"Success",act):
					print("False removal success of Key {} in Session {}".format(key,i+1), file=f)
			if key in table:
				table.remove(key)
				if re.search(r"Fail",act):
					print("False removal failure of Key {} in Session {}".format(key,i+1), file=f)
		elif re.match(r"Contains",act):
			key = int(re.search(r"[0-9]+",act).group())
			if key not in table:
				if re.search(r"Success",act):
					print("False contains success of Key {} in Session {}".format(key,i+1), file=f)
			if key in table:
				if re.search(r"Fail",act):
					print("False contains failure of Key {} in Session {}".format(key,i+1), file=f)
		else: #end of session, non-op line encountered
			break

f.seek(0)
if f.read().strip().strip("\n") == "":
	print("Everything's all right!",file=f)
f.close()