f = open("stopwords2.txt")
words = f.read().replace('\n', ' ')
print words.split("\n")