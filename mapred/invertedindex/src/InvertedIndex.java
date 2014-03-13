package edu.utk.eecs;

import java.io.*;
import java.util.*;

import org.apache.hadoop.fs.*;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.io.*;
import org.apache.hadoop.mapred.*;
import org.apache.hadoop.util.*;

/*  Get a count of each word in a set of text files.
    Split each word on any punctuation found by regex.
    Trim leading and trailing whitespace of these split words
    Make all these words lowercase
    Recursively word count on the split words
 
    i.e. if Shakespear wrote, "MY Dar'ling dear",
    we would get "my", "dar", "ling", and "dear"
 */
public class FilterAndWordCount {

//	static List<String> stopwords = Arrays.asList("I  a  about  an  are as  at  be  by  com  for  from how in  is  it  of  on  or  that the  this to  was  what  when where the who  will  with the www".split(" "));
	
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
                    word.set(clean);
                    output.collect(word, one);
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
			
		JobConf conf = new JobConf(FilterAndWordCount.class);
		conf.setJobName("FilterAndWordCount");
		
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
