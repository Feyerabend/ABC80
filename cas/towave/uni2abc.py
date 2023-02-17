import sys
import getopt


def svenska(content):
    ncontent = []
    for line in content:
        line.encode().decode('unicode-escape')

        # svensk teckenkod 7 bitar ISO ESC 2/8 4/7
        # ÄÖÅÉ äöåé
        line = line.replace(u'Ä', '[')
        line = line.replace(u'Ö', '\\')
        line = line.replace(u'Å', ']')
        line = line.replace(u'È', '@') # ?? right
        line = line.replace(u"ä", '{')
        line = line.replace(u'ö', '|')
        line = line.replace(u'å', '}')
        line = line.replace(u'é', '`') # ?? right

        line = line.replace(u'¤', '$') # ?? right

        line.encode('ascii', 'ignore')
        ncontent.append(line)
    return ncontent

# \n = chr 10 = LINE FEED
# \r = chr 13 = CARRIAGE RETURN
def linereplacement(content):
    ncontent = []
    for line in content:
        line = line.replace('\n', '\r') # return for line feed
        line = line.replace('\r\r', '\r') # then, no doubles
        ncontent.append(line)
    return ncontent

# stuff
def prepare(content):
    content = svenska(content)
    content = linereplacement(content)
    return content

# convert from Unicode UTF-8 to ASCII for ABC80
def convertabc(inputfile, outputfile, verbose):

    with open(inputfile, 'r', encoding = 'utf-8') as f:
        content = f.readlines()
    content = prepare(content)
    with open(outputfile, 'w', encoding = 'ascii') as f:
        f.write(''.join(content)) # introduces new lines?

# call with parsing of args to conversion
def main(argv):
    inputfile = ''
    outputfile = ''
    verbose = 0

    try:
        opts, args = getopt.getopt(argv,"vhi:o:",["ifile=","ofile="])
    except getopt.GetoptError:
        print('uni2abc.py -i <inputfile> -o <outputfile>')
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-v':
            verbose = 1
        if opt == '-h':
            print('usage: uni2abc.py -i <inputfile> -o <outputfile>')
            sys.exit()
        elif opt in ("-i", "--ifile"):
            inputfile = arg
        elif opt in ("-o", "--ofile"):
            outputfile = arg

    if verbose == 1:
        print("converting ..")
    convertabc(inputfile, outputfile, verbose)
    if verbose == 1:
        print("done.")


if __name__ == "__main__":
   main(sys.argv[1:])
