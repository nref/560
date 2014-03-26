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

		public List<String> readLines(FSDataInputStream fileInputStream) throws IOException {
			//FileReader fileReader = new FileReader(filename);
			InputStreamReader hdfsInputStream = new InputStreamReader(fileInputStream);
			BufferedReader bufferedReader = new BufferedReader(hdfsInputStream);
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

			System.out.println(line);
			String lineNum = line.substring(0, line.indexOf(' ')).trim();
			String remainder = line.substring(line.indexOf(' ')).trim();
			System.out.println(lineNum);
			System.out.println(remainder);

			Configuration config = new Configuration();
			config.set("fs.default.name","hdfs://hydra29.eecs.utk.edu:51211");
			FileSystem dfs = FileSystem.get(config);
			Path src = new Path(dfs.getWorkingDirectory()+"/input/stopwords.txt");
			FSDataInputStream fs = dfs.open(src);
			
			List<String> stopwords = readLines(fs);

			FileSplit fileSplit = (FileSplit)reporter.getInputSplit();
			String filename = fileSplit.getPath().getName();
			docID.set(filename+":"+lineNum);
            StringTokenizer tokenizer = new StringTokenizer(remainder);

            while (tokenizer.hasMoreTokens()) {
				word.set(tokenizer.nextToken());

                // Split this word on all punctuation characters
                // Also trim leading and trailing whitespace
                // Also make lower-case
				// TODO: This is not cleaning the whole line of punctuation"
				if( stopwords.contains(word.toString()) ) {
					continue;
				} else {
					String clean = word.toString().replaceAll("[\\p{P}]", " ").trim().toLowerCase();
					List<String> parts = new ArrayList<String>(Arrays.asList(clean.split(" ")));

					// Each individual part needs to be mapped
					for (String part : parts) {
						word.set(part);
						output.collect(word, docID);
					}
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
		conf.setCombinerClass(Reduce.class);
		conf.setReducerClass(Reduce.class);
		
		conf.setInputFormat(TextInputFormat.class);
		conf.setOutputFormat(TextOutputFormat.class);
		
		FileInputFormat.setInputPaths(conf, new Path(args[0]));
		FileOutputFormat.setOutputPath(conf, new Path(args[1]));

		
		JobClient.runJob(conf);
    }
}
