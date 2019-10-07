import numpy as np, argparse, sys
parser = argparse.ArgumentParser()
parser.add_argument('-n',type=int,dest='nums')
parser.add_argument('-f',type=str,dest='file')
args = vars(parser.parse_args(sys.argv[1:]))
n = args['nums']
out = open(args['file'],'w') if args['file'] else sys.stdout
b = np.random.random_integers(0,30000,size=(n,n))
b_symm = (b + b.T)//2
print(n,file=out)
for i in range(np.size(b_symm,0)):
	for j in range(np.size(b_symm,1)):
		if i==j:
			print(0,' ',end='',file=out)
		else:
			print(b_symm[i][j],' ',end='',file=out)
	print('',file=out)
out.close()
