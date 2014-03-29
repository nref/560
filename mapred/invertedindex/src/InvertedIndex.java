package edu.utk.eecs;

import java.io.*;
import java.util.*;
import java.net.*;

import org.apache.hadoop.fs.*;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.io.*;
import org.apache.hadoop.mapred.*;
import org.apache.hadoop.mapreduce.Mapper.*;
import org.apache.hadoop.util.*;

/*
 * Build the inverted index
 */
public class InvertedIndex {
	
	public static class Map extends MapReduceBase implements Mapper<LongWritable, Text, Text, Text> {
        
        private String username = new com.sun.security.auth.module.UnixSystem().getUsername();
        private Text word = new Text();
		private Text docID = new Text();
		private HashMap<String, Object> stopwords = null;

		public void readLines() throws IOException {
			Path pt = new Path("/user/"+username+"/other/stopwords.txt");
			FileSystem fs = FileSystem.get(new Configuration());
            
			BufferedReader br = new BufferedReader(new InputStreamReader(fs.open(pt)));
			List<String> lines = new ArrayList<String>();
            
			String line = null;
			stopwords = new HashMap<String,Object>();
            
			//System.out.println("Produced stopwords list");
            
			while ((line = br.readLine()) != null && line.trim().length() > 0) {
				stopwords.put(line, null);
			}
			br.close();
		}


		public void map(LongWritable key, Text value, OutputCollector<Text, Text> output, Reporter reporter) throws IOException {
			readLines();    // Set the global stopwords
			String line = value.toString();

			String lineNum = line.substring(0, line.indexOf(' '));
			String remainder = line.substring(line.indexOf(' ')).trim();

			FileSplit fileSplit = (FileSplit)reporter.getInputSplit();
			String filename = fileSplit.getPath().getName();

			StringTokenizer tokenizer = new StringTokenizer(remainder);

			int fieldNum = 1;
			while (tokenizer.hasMoreTokens()) {
				word.set(tokenizer.nextToken());

				// Split this word on all punctuation characters
				// Also trim leading and trailing whitespace
				// Also make lower-case
				// TODO: This is not cleaning the whole line of punctuation"

				String clean = word.toString().replaceAll("[\\p{P}]", " ").trim().toLowerCase();
				List<String> parts = new ArrayList<String>(Arrays.asList(clean.split(" ")));

				// Each individual part needs to be mapped

				for (String part : parts) {
					if(stopwords.containsKey(part)) {
						//System.out.println("Got stopword: " + part);
						continue;
					}
					word.set(part);
					docID.set(filename + ":" + lineNum + ":" + Integer.toString(fieldNum));
					++fieldNum;

					output.collect(word, docID);
				}
			}
		}
	}

	public static class Reduce extends MapReduceBase implements Reducer<Text, Text, Text, Text> {
		public void reduce(Text key, Iterator<Text> values, OutputCollector<Text, Text> output, Reporter reporter) throws IOException {
			String docIDs = "";

            boolean first = true;
            
			while(values.hasNext()){
				Text val = values.next();
				if(val.toString() != "") {
                    if (!first) {
                        docIDs += ",";
                    }
                    first = false;
					docIDs += val.toString();
				}
			}
			output.collect(key, new Text(docIDs));
		}
	}

	public static void main(String[] args) throws Exception {

		JobConf conf = new JobConf(InvertedIndex.class);
		conf.setJobName("InvertedIndex");

		conf.setOutputKeyClass(Text.class);
		conf.setOutputValueClass(Text.class);

		conf.setMapperClass(Map.class);
		conf.setReducerClass(Reduce.class);

		conf.setInputFormat(TextInputFormat.class);
		conf.setOutputFormat(TextOutputFormat.class);

		FileInputFormat.setInputPaths(conf, new Path(args[0]));
		FileOutputFormat.setOutputPath(conf, new Path(args[1]));

		JobClient.runJob(conf);
	}
}
