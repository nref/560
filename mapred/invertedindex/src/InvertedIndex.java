package edu.utk.eecs;

import java.io.*;
import java.util.*;

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
        
        private Text word = new Text();
		private Text docID = new Text();
		private HashMap<String, Object> stopwords = null;

		public void setup(Context context) {
			Configuration conf = context.getConfiguration();
			//hardcoded or set it in the jobrunner class and retrieve via this key
			String location = conf.get("fs.default.name");
			if (location != null) {
				BufferedReader br = null;
				try {
					FileSystem fs = FileSystem.get(conf);
					//Hard code the path to the stop words file
					Path path = new Path(location+"/other/stopwords.txt");
					if (fs.exists(path)) {
						stopwords = new HashMap<String, Object>();
						FSDataInputStream fis = fs.open(path);
						br = new BufferedReader(new InputStreamReader(fis));
						String line = null;
						while ((line = br.readLine()) != null && line.trim().length() > 0) {
							stopwords.put(line, null);
						}
					}
				}
				catch (IOException e) {
					e.printStackTrace();
				} 
			}
		}

		public List<String> readLines(String filename) throws IOException {
			FileReader fileReader = new FileReader(filename);
			BufferedReader bufferedReader = new BufferedReader(fileReader);
			List<String> lines = new ArrayList<String>();
			String line = null;
			while ((line = bufferedReader.readLine()) != null) {
				lines.add(line);
			}
			bufferedReader.close();
			return lines;
		}


		public void map(LongWritable key, Text value, OutputCollector<Text, Text> output, Reporter reporter) throws IOException {
			String line = value.toString();
			//            String lineNum = key.toString(); TODO: If we have time
			String lineNum = line.substring(0, line.indexOf(' '));
			String remainder = line.substring(line.indexOf(' ')).trim();

			FileSplit fileSplit = (FileSplit)reporter.getInputSplit();
			String filename = fileSplit.getPath().getName();
			//			docID.set(filename+":"+lineNum);

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
			while(values.hasNext()){
				Text val = values.next();
				if(val.toString() != "") {
					docIDs += "," + val.toString();
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
		//		conf.setCombinerClass(Reduce.class);
		conf.setReducerClass(Reduce.class);

		conf.setInputFormat(TextInputFormat.class);
		conf.setOutputFormat(TextOutputFormat.class);

		FileInputFormat.setInputPaths(conf, new Path(args[0]));
		FileOutputFormat.setOutputPath(conf, new Path(args[1]));

		JobClient.runJob(conf);
	}
}
