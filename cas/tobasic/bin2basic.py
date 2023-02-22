import getopt
import math
import sys
import re


def svenska(ascii):

    # control characters
    if (ascii == 13):
        return '\r\n'
    if (ascii < 32): # ctrl
        return ''
    if (ascii == 127): # DEL
        return ''
    if (ascii > 128): # 7 bit ASCII
        return ''

    char = chr(ascii & int(0xff))
    if (ascii == 64): # @
        char = 'É'
    if (ascii == 91): # [
        char = 'Ä'
    if (ascii == 92): # \
        char = 'Ö'
    if (ascii == 93): # ]
        char = 'Å'
    if (ascii == 94): # ^
        char = 'Ü'
    if (ascii == 96): # `
        char = 'é'
    if (ascii == 123): # {
        char = 'ä'
    if (ascii == 124): # |
        char = 'ö'
    if (ascii == 125): # }
        char = 'å'
    if (ascii == 126): # ~
        char = 'ü'

    if (ascii == 36): # $
        char = '¤'
    return char

def checkforname(block):
    return re.fullmatch('1{24}', block[0:24])

def nameblock(block):
    fullname = block[24:112]
    byts = [fullname[i: i + 8] for i in range(0, len(fullname), 8)]
    name = ''
    for i in range(len(byts)):
        name += svenska(int((byts[i])[::-1], 2))
    return name[0:8] + '.' + name[8:11]


def checkforblocknumber(block):
    return int((block[8:24])[::-1], 2)

def datablock(block):
    null = block[0:8]
    userdata = block[24:2048]
    byts = [userdata[i: i + 8] for i in range(0, len(userdata), 8)]
    userstring = ''
    for i in range(len(byts)):
        userstring += svenska(int((byts[i])[::-1], 2))
    return userstring


def checksums(block):

    # can't check other block sizes
    if (len(block) != 2072):
        return False

    data = block[0:2048]
    checksum = block[2056:2072]

    # split into bytes from binary string
    byts = [data[i: i + 8] for i in range(0, len(data), 8)]

    calcsum = 3 # ETX
    for i in range(len(byts)):
        calcsum += int((byts[i])[::-1], 2)

    cs = int((checksum)[::-1], 2)

    if (calcsum == cs):
        return True
    print('ERR 35\n') # CHECKSUMMAFEL VID LÄSNING
    return False


# stuff
def prepare(content, verbose):
    content = ''.join(content)

    sync = '01101000' # 16h in reverse
    stx = '01000000' # 2h in reverse
    blocks = re.split('0{256}' + sync + sync + sync + stx, content)

    out = ''
    no = 0;
    for i in range(len(blocks)):
        blk = blocks[i]
        if (checksums(blk)):
            if (checkforname(blk)):
                if verbose == 1:
                    print("name: ", nameblock(blk))
            else:
                nb = checkforblocknumber(blk)
                if (no == nb):
                    if verbose == 1:
                        if no == 0: # first block after name
                            print("program:")
                    out += datablock(blk)
                    no += 1
                else:
                    print('ERR 38\n') # FELAKTIGT RECORDFORMAT eller ERR38 RECORDNUMMER UTANFÖR FILEN?
        else:
            print('ERR 37\n') # FELAKTIGT RECORDFORMAT # print(datablock(blk)) # the non-checkable block?
    return out


# convert from "binary" textfile to program text for ABC80
def convertbin(inputfile, outputfile, verbose):

    with open(inputfile, 'r', encoding = 'ascii') as f:
        content = f.readlines()

    content = prepare(content, verbose)
    if verbose == 1:
        print(content)

    with open(outputfile, 'w', encoding = 'utf-8') as f:
        f.write(content)

# call conversion
def main(argv):
    inputfile = ''
    outputfile = ''
    verbose = 0

    try:
        opts, args = getopt.getopt(argv,"vhi:o:",["ifile=","ofile="])
    except getopt.GetoptError:
        print('bin2basic.py -i <inputfile> -o <outputfile>')
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-v':
            verbose = 1
        if opt == '-h':
            print('usage: bin2basic.py -i <inputfile> -o <outputfile>')
            sys.exit()
        elif opt in ("-i", "--ifile"):
            inputfile = arg
        elif opt in ("-o", "--ofile"):
            outputfile = arg

    if verbose == 1:
        print("converting ..")
    convertbin(inputfile, outputfile, verbose)
    if verbose == 1:
        print("done.")


if __name__ == "__main__":
   main(sys.argv[1:])
