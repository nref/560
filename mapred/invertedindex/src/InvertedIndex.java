package edu.utk.eecs;

import java.io.*;
import java.util.*;

import org.apache.hadoop.fs.*;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.io.*;
import org.apache.hadoop.mapred.*;
//import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Mapper.*;
import org.apache.hadoop.util.*;

/*
 */
public class InvertedIndex {
	
	public static class Map extends MapReduceBase implements Mapper<Text, Text, Text, Text> {
        
        private Text word = new Text();
		private Text docID = new Text();
        
        public void map(Text key, Text value, OutputCollector<Text, Text> output, Reporter reporter) throws IOException {
            
            String line = value.toString();
			String lineNum = line.substring(0, line.indexOf(' '));
			String remainder = line.substring(line.indexOf(' ')).trim();

			FileSplit fileSplit = (FileSplit)reporter.getInputSplit();
			String filename = fileSplit.getPath().getName();
			docID.set(filename+":"+lineNum);
			
            StringTokenizer tokenizer = new StringTokenizer(remainder);

            while (tokenizer.hasMoreTokens()) {
                
                word.set(tokenizer.nextToken());
                
                String clean = word.toString().replaceAll("[\\p{P}]", " ").trim().toLowerCase();
                List<String> parts = new ArrayList<String>(Arrays.asList(clean.split(" ")));

                // If there is only one word in the split
				// TODO: filter stopwords
                if (1 == parts.size()) {
                    word.set(clean);
                    output.collect(word, docID);
                }
                
                // Map the split words
                else {
                    for (String part : parts) {
                        Text part_Text = new Text(part);
                        map(key, part_Text, output, reporter);
                    }
                }
            }
        }
    }

/*
    public static class Reduce extends MapReduceBase implements Reducer<Text, IntWritable, Text, IntWritable> {
        public void reduce(Text key, Iterator<IntWritable> values, OutputCollector<Text, IntWritable> output, Reporter reporter) throws IOException {
            
            output.collect(key, new IntWritable(sum));
        }
    }
	*/

    public static void main(String[] args) throws Exception {
			
		JobConf conf = new JobConf(InvertedIndex.class);
		conf.setJobName("InvertedIndex");
		
//		conf.setOutputKeyClass(Text.class);
//		conf.setOutputValueClass(Text.class);
		
		conf.setMapperClass(Map.class);
//		conf.setCombinerClass(Reduce.class);
//		conf.setReducerClass(Reduce.class);
		
		conf.setInputFormat(TextInputFormat.class);
		conf.setOutputFormat(TextOutputFormat.class);
		
		FileInputFormat.setInputPaths(conf, new Path(args[0]));
		FileOutputFormat.setOutputPath(conf, new Path(args[1]));
		
		JobClient.runJob(conf);
    }
}
