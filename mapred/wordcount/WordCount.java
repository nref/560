package edu.utk.eecs;

import java.io.*;
import java.util.*;

import org.apache.hadoop.fs.*;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.io.*;
import org.apache.hadoop.mapred.*;
import org.apache.hadoop.util.*;

public class WordCount {

	static List<String> stopwords = Arrays.asList("I  a  about  an  are as  at  be  by  com  for  from how in  is  it  of  on  or  that the  this to  was  what  when where the who  will  with the www".split(" "));
	
	public static class Map extends MapReduceBase implements Mapper<LongWritable, Text, Text, IntWritable> {
      private final static IntWritable one = new IntWritable(1);
      private Text word = new Text();
      
      public void map(LongWritable key, Text value, OutputCollector<Text, IntWritable> output, Reporter reporter) throws IOException {
        String line = value.toString();
        StringTokenizer tokenizer = new StringTokenizer(line);
    	
        while (tokenizer.hasMoreTokens()) {
        	
            word.set(tokenizer.nextToken());
            
            // Split this word on all punctuation characters
            // Also trim leading and trailing whitespace
            // Also make lower-case
            String clean = word.toString().replaceAll("[\\p{P}]", " ").trim().toLowerCase();

            List<String> parts = new ArrayList<String>(Arrays.asList(clean.split(" ")));

            // If there is only one word in the split
            if (parts.size() == 1) {
            	String part = parts.get(0);
            	
            	// Skip words of 2 or less and stop words
            	if (part.length() < 3 || stopwords.contains(part)) 
            		return;
            	
                word.set(clean);
                output.collect(word, one);
            }
            
            // Map the split words
            else {
	            for (String part : parts) {
	            	
	            	// Skip words of 2 or less and stop words
	            	if (part.length() < 3 || stopwords.contains(part)) 
	            		continue;
	            	
	            	Text part_Text = new Text(part);
	            	map(key, part_Text, output, reporter);
	            }
            }
        }
      }
    }

    public static class Reduce extends MapReduceBase implements Reducer<Text, IntWritable, Text, IntWritable> {
      public void reduce(Text key, Iterator<IntWritable> values, OutputCollector<Text, IntWritable> output, Reporter reporter) throws IOException {
        int sum = 0;
        while (values.hasNext()) {
          sum += values.next().get();
        }
        output.collect(key, new IntWritable(sum));
      }
    }

    public static void main(String[] args) throws Exception {
			
		JobConf conf = new JobConf(WordCount.class);
		conf.setJobName("wordcount");
		
		conf.setOutputKeyClass(Text.class);
		conf.setOutputValueClass(IntWritable.class);
		
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
