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
        
        public void map(LongWritable key, Text value, OutputCollector<Text, Text> output, Reporter reporter) throws IOException {
            
            String line = value.toString();
//            String lineNum = key.toString(); TODO: If we have time
			String lineNum = line.substring(0, line.indexOf(' '));
			String remainder = line.substring(line.indexOf(' ')).trim();

			FileSplit fileSplit = (FileSplit)reporter.getInputSplit();
			String filename = fileSplit.getPath().getName();
			docID.set(filename+":"+lineNum);
			
            StringTokenizer tokenizer = new StringTokenizer(remainder);

            while (tokenizer.hasMoreTokens()) {
                
                word.set(tokenizer.nextToken());
                output.collect(word, docID);

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
		
		conf.setOutputKeyClass(Text.class);
		conf.setOutputValueClass(Text.class);
		
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
