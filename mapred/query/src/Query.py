""" 
CS560 Mapreduce lab submission
29 March 2014
Doug Slater, mailto:cds@utk.edu
Department of Electrical Engineering and Computer Science
University of Tennessee, Knoxville
http://www.eecs.utk.edu/
"""
import sys, os, itertools

# ASCII escape sequences for terminal colors
HEADER = '\033[95m'
OKBLUE = '\033[94m'
OKGREEN = '\033[92m'
WARNING = '\033[93m'
FAIL = '\033[91m'
ENDC = '\033[0m'

AND, OR, NOT, QUOTE = 0, 1, 2, 3
SEARCHMODES = ["AND", "OR", "NOT", "QUOTE"]

# Configuration
NUM_INDEX_PREVIEWS = 10
NUM_INDEX_ENTRY_PREVIEW_CHARS = 75
INDEX_DIR = "../../invertedindex/output/"
INPUT_DIR = "../../invertedindex/input/"

indent = " " * 4
w = sys.stdout.write

def readFiles():
    """
    Read all the lines in INDEX_DIR which start with "part-", i.e.
    the Hadoop default output prefix.
    """
    lines = []

    # Get a list of all files in INDEX_DIR which start with "part-"
    files = [f for f in os.listdir(INDEX_DIR) if f.startswith("part-")]

    # Read in lines from all the files
    for f in files:
        with open(INDEX_DIR+f) as f:
            lines += f.readlines()

    return lines

def consecutive(l):
    """
    Determine if any two consecutive docIDs in list l
    are indexes to two consecutive words
    """
    for first, second in itertools.izip(l, l[1:]): # for each pair
        if first[2]+1 != second[2]: return False
    return True

def ListRemoveDuplicates(l1, l2):
    """
    Remove duplicate entries from list l1 and remove the entries
    of the same index in list l2

    Example: 

    for i in range(len(l1)):
        l1[i], l2[i] = ListRemoveDuplicates(l1[i], l2[i])
    """
    
    d = {}      # Remember values of elements already seen
    to_rm = []  # Indices of elements to be removed
    
    for j in range(len(l1)):
        d[l1[j]] = 0 if not l1[j] in d else to_rm.append(j)
    
    # Remove from both lists
    l1 = [a for b, a in enumerate(l1) if b not in to_rm]
    l2 = [a for b, a in enumerate(l2) if b not in to_rm]
    
    return l1, l2

def buildInvertedIndex(lines):
    """
    Produce an in-memory representation of the Hadoop-produced inverted index,
    """
    keys = []
    index = {}
    
    # Split each line into a key and a value, put in dictionary
    for i in range(len(lines)):
        elems = lines[i].replace('\n', '').split("\t")
        
        value = filter(None, elems[1].split(",")) # Filter removes '' list elements

        keys.append(elems[0])
        index[keys[i]] = value
    
    return keys, index

def buildForwardIndex(lines):
    """
    Given the lines from the Hadoop-produced inverted index,
    construct a forward index.
    """
    keys = []
    index = {}

    for i in range(len(lines)):
        elems = lines[i].replace('\n', '').split("\t")
        
        word = elems[0]
        docIDs = elems[1].split(",")

        for docID in docIDs:
            locators = docID.split(":") # filename:lineNum:fieldNum
            
            if len(locators) < 3: # not a valid docID
                continue
        
            toAppend = [locators[1], locators[2]]
            
            try: index[locators[0]]
            except KeyError:    # Never seen this file before
                keys.append(locators[0])
                index[locators[0]] = {}
                
            try: index[locators[0]][word].append(toAppend)
            except KeyError:    # Never seen this word before
                index[locators[0]][word] = [toAppend]

    return keys, index

def previewIndex(keys, index):
    """
    Show a preview of the inverted index
    """
    
    for i in range(len(keys[:NUM_INDEX_PREVIEWS])):
        value = str(index[keys[i]])
        preview = value[:NUM_INDEX_ENTRY_PREVIEW_CHARS]
        
        w("    " + HEADER + keys[i] + ENDC + " " + OKGREEN + preview + ENDC)
          
        numLeft = max(len(value)-NUM_INDEX_ENTRY_PREVIEW_CHARS, 0)
        
        if numLeft > 0: w(" ...")
        w("\n")
    
    numLeft = max(len(keys)-NUM_INDEX_PREVIEWS, 0)
    if numLeft > 0: w("    ... and %d more ...\n\n" % numLeft)
    else: w("\n")

def parseUserQueries(args):
    """
    Loop through the command-line args and group by AND, OR, NOT
    It only builds n-term homogenous combinations, e.g.

    "term1 AND term2"           works
    "term1 AND term2 AND term3" works
    "term1 AND term2 OR term3"  the OR will be treated as a search term, not a command
    """
    terms = []
    i = 0
    nargs = len(args)


    while i < nargs:
        term = args[i]
        while i+2 < nargs and args[i+1] == "AND":
            term += " AND " + args[i+2]
            i+=2
        
        while i+2 < nargs and args[i+1] == "OR":
            term += " OR " + args[i+2]
            i+=2
        
        while i+2 < nargs and args[i+1] == "NOT":
            term += " NOT " + args[i+2]
            i+=2
        
        terms.append(term)
        i+=1

    terms = list(set(terms)) # Remove duplicates
    return sorted(terms)     # Sort

def getQueryWords(term, mode):
    """
    Given a string, split it either on spaces if mode = QUOTE, else split
    on the mode ("AND", "OR", or "NOT")
    """
    if mode == QUOTE:
        delim = " "
    else: delim = SEARCHMODES[mode]

    return [word.strip().lower() for word in term.split(delim)]

def keysOK(words, mode, invKeys):
    """
    Make a quick loop through the keys of the inverted index.

    For AND and QUOTE,  return True if all of search words are found,     else False
    For OR,             return True if any of the search words are found, else False
    For NOT,            return True if the first search word is found,    else False
    """
    
    if mode == AND or mode == QUOTE:
        for word in words:
            if not word in invKeys:
                return False
        return True

    elif mode == OR:
        for word in words:
            if word in invKeys:
                return True
        return False

    elif mode == NOT:
        if not words[0] in invKeys:
            return False
        return True

def getSearchMode(term):
    """
    Return the first occurence of in this order: AND, OR, NOT
    If none found, return QUOTE, e.g.

    "Hello AND World OR Bob"    returns AND
    "Hello OR World"            returns OR
    "Hello World"               returns QUOTE
    "Hello"                     returns QUOTE
    """
    for searchmode in SEARCHMODES:
        if searchmode in term:
            return SEARCHMODES.index(searchmode)
    return QUOTE

def DocIDsTrimFieldNums(docIDs):
    """
    Remove the fieldNum from each of the docIDs, 
    i.e. the third field in the text split by ":"
    """
    return [":".join(docID.split(":")[:-1]) for docID in docIDs]

def AllSublistsSetOperation(l, op = AND):
    """
    Return the set operation of all sublists in a list.
    http://stackoverflow.com/questions/10066642
    """

    r = set(l[0])
    for s in l[1:]:
        if op == AND:
            r.intersection_update(s)
        elif op == OR:
            r.update(s) # Same as union_update()
        elif op == NOT:
            r.difference_update(s)
    return r

def SearchFileAND(forwardIdx, query):
    """
    Find all files which contain all the terms in the query
    """

    results = []

    # {filename: { word: [[fieldNum,word],[fieldNum, word],...]}}
    for key, value in forwardIdx.iteritems():
        failed = False
        locations = []

        for term in query:
            try:
                locations += value[term]
            except KeyError:
                failed = True
                    
        if not failed:
            for location in locations:
                results.append([key, int(location[0]), int(location[1])])

    return results

def DocIDToInt(docID):
    ret = [s.split(":") for s in list(docID)]
    ret = [[s[0],int(s[1]),-1] for s in ret]    # Convert lineNum to int, add dummy fieldNum

    return ret

def DocIDToStr(docID):
    return [d[0]+":"+str(d[1]) for d in docID]

def Search(invKeys, invIdx, query, mode = AND):
    """
    File all lines which contain all (mode = AND) 
    or any (mode = OR) of the terms in the query
    """
    docIDs = []     # Search results per term
    
    if not keysOK(query, mode, invKeys):
        return None
    
    if mode == QUOTE: return SearchQUOTE(invKeys, invIdx, query)
    elif mode == NOT: return SearchNOT(invKeys, invIdx, query)

    # Since we only care about line numbers,
    # remove fieldNum from each of the docIDs
    for term in query:
        try: docIDs.append(DocIDsTrimFieldNums(invIdx[term]))
        except KeyError: return None

    searchResult = AllSublistsSetOperation(docIDs, mode)    # Works for AND, OR
    return DocIDToInt(list(searchResult))

def SearchQUOTE(invKeys, invIdx, query):
    """
    Find all occurences of the quoted phrase in the query
    Works for single words, e.g. "Hello" as well as "Hello World"
    
    Note: QUOTE is a stricter version of AND
    in which the terms have a strict consecutive ordering.
    """
    queryResults = []
    matches = []
    
    # Can't use Search() here because it returns dummy fieldNums
    for term in query:
        try:
            docIDs = []
            for docID in invIdx[term]:
                
                info = docID.split(':')
                
                if len(info) < 3: # not a valid docID
                    continue
                
                # Found a term
                docIDs.append([info[0], int(info[1]), int(info[2])])
            queryResults.append(docIDs)
    
        # The term was not in the index
        except KeyError:
            return None

    # Flatten one level
    r = list(itertools.chain.from_iterable(queryResults))

    for combo in itertools.combinations(r, len(query)): # All n-combinations of the docIDs
        # We have a perfect quote match if...
        #   the filenames match and
        #   the line numbers are the same and
        #   the words are consecutive
        
        fnames = [docID[0] for docID in combo]
        linNums = [docID[1] for docID in combo]

        if len(set(fnames))  > 1:  continue
        if len(set(linNums)) > 1:  continue
        if not consecutive(combo): continue
        
        matches += [docID for docID in combo]

    return matches

def SearchNOT(invKeys, invIdx, query):
    """
    Find all occurences of the first term in a line where
    the remaining terms are not.
    
    firstTerm = query[0]
    remainingTerms = query[1:]
    """

    # Exit if first word in not in keys
    try: firstTermDocIDs = invIdx[query[0]]
    except KeyError: return None

    firstTermResults = Search(invKeys, invIdx, [query[0]])  # docIDs for the first term
    remainTermResults = Search(invKeys, invIdx, query[1:])  # docIDs for the remaining terms
    
    # Return success if none of the remaining words in keys
    if remainTermResults is None:
        return firstTermResults

    first = DocIDToStr(firstTermResults)
    remain = DocIDToStr(remainTermResults)

    searchResults = AllSublistsSetOperation([first, remain], op = NOT)

    return DocIDToInt(list(searchResults))

def printSnippet(fileName, lineNum, fieldNum):
    """
    Print a preview snippet of the found text
    """
    fLines = []
    
    # Read the relevant file
    with open(INPUT_DIR + fileName) as f:
        fLines += f.readlines()
    
    # Get the relevant line, split into fields
    theLine = fLines[lineNum-1].replace('\n', '').split(' ')
    
    # Configure length of the text snippet
    s = 4
    snippetBegin = 0 if fieldNum - s < 0 else fieldNum - s
    snippetEnd = len(theLine) if fieldNum + s > len(theLine) else fieldNum + s
    
    if fieldNum < 0:
        snippetBegin = 0
        snippetEnd = snippetBegin + s
    
    # Print the snippet
    
    # Color quoted text green
    w(": \"..." + OKGREEN)
    
    # For each word in the snippet
    for i in range(snippetBegin, snippetEnd):
        
        # Color only the matching field blue
        if i == fieldNum-1: w(OKBLUE)
        w(theLine[i] + ' ' + OKGREEN)
    
    # Back to normal color
    w(ENDC +"... \"")

def printSearchResults(results):
    """
    Pretty-print the results of a search.
    """
    if results is None or len(results) == 0:
        w(indent + FAIL + "NOT found" + ENDC)
    
    else:
        for result in results:
            
            fileName,lineNum,fieldNum = result[0],result[1],result[2]
            
            # Show where the result was found
            w("\n" + indent + "found in " + fileName +
              ", line "   + str(lineNum))
              
            if fieldNum > 0:
                w(", field "  + str(fieldNum))
            
            printSnippet(fileName, lineNum, fieldNum)

    w("\n\n")

def printSearchQuery(query, words, mode):
    """
    Print pre-search information about a user's search query.
    """
    w(HEADER + query + ENDC + "\n")
    w(indent + "Terms in this query: %s\n" % words)
    w(indent + "Mode: %s\n" % SEARCHMODES[mode])

def main():
    w("Loading the inverted index...")
    lines = readFiles()
    
    invKeys, invIdx         = buildInvertedIndex(lines)
    forwardKeys, forwardIdx = buildForwardIndex(lines)
    
    w(OKGREEN + "OK\n" + ENDC)
    w("Here's a preview of the inverted index:\n")
    previewIndex(invKeys, invIdx)
    
    w("Here's a preview of the forward index (generated from the inverted index):\n")
    previewIndex(forwardKeys, forwardIdx)

    w("Parsing your query...\n\n")
    queries = parseUserQueries(sys.argv[1:])
    
    print "Given arguments: %s" % sys.argv[1:]
    print "Parsed into search queries %s\n" % queries
    
    for query in queries:

        mode = getSearchMode(query)
        words = getQueryWords(query, mode)
        printSearchQuery(query, words, mode)
        
        result = Search(invKeys, invIdx, words, mode)
        printSearchResults(result)

if __name__ == "__main__":
    main()