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
        
        private Text word = new Text();
		private Text docID = new Text();
		private HashMap<String, Object> stopwords = null;

/*
		@Override
		public void configure(JobConf job){
			//hardcoded or set it in the jobrunner class and retrieve via this key
			String fs_uri = "hdfs://hydra29.eecs.utk.edu:51211";
			System.out.println("In the setup node");
			BufferedReader br = null;
			try {
				FileSystem fs = FileSystem.get(new URI(fs_uri), null, null);
				//Hard code the path to the stop words file
				Path path = new Path(fs.getHomeDirectory()+"/other/stopwords.txt");
				if (fs.exists(path)) {
					System.out.println("Actually produced a stopwords list");
					stopwords = new HashMap<String, Object>();
					FSDataInputStream fis = fs.open(path);
					br = new BufferedReader(new InputStreamReader(fis));
					String line = null;
					while ((line = br.readLine()) != null && line.trim().length() > 0) {
						stopwords.put(line, null);
					}
				}
			} catch (IOException e) {
				e.printStackTrace();
		    } catch (URISyntaxException e) {
				e.printStackTrace();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
		*/

		public void readLines() throws IOException {
			Path pt = new Path("hdfs://hydra29.eecs.utk.edu:51211/user/ccraig7/other/stopwords.txt");
			FileSystem fs = FileSystem.get(new Configuration());
			BufferedReader br = new BufferedReader(new InputStreamReader(fs.open(pt)));
			List<String> lines = new ArrayList<String>();
			String line = null;
			stopwords = new HashMap<String,Object>();
			System.out.println("Actually produced a stopwords list");
			while ((line = br.readLine()) != null && line.trim().length() > 0) {
				stopwords.put(line, null);
			}
			br.close();
		}


		public void map(LongWritable key, Text value, OutputCollector<Text, Text> output, Reporter reporter) throws IOException {
			readLines(); //set the global stopwords
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
						System.out.println("Got stopword: " + part);
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
			//Configuration config = new Configuration();
			//config.set("fs.default.name","hdfs://hydra29.eecs.utk.edu:51211");
			//FileSystem dfs = FileSystem.get(config);
			//Path src = new Path(dfs.getWorkingDirectory()+"/input/stopwords.txt");
			//FSDataInputStream fs = dfs.open(src);
			//List<String> stopwords = readLines(fs);
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
		conf.setReducerClass(Reduce.class);

		conf.setInputFormat(TextInputFormat.class);
		conf.setOutputFormat(TextOutputFormat.class);

		FileInputFormat.setInputPaths(conf, new Path(args[0]));
		FileOutputFormat.setOutputPath(conf, new Path(args[1]));

		JobClient.runJob(conf);
	}
}
