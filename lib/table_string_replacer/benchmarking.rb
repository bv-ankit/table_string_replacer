module TableStringReplacer
  # Benchmarking utilities for TableStringReplacer
  module Benchmarking
    # Compares performance between this gem and Ruby's native string replacement
    #
    # @param str [String] Input string to test against
    # @param replacements [Array<Array<String, String>>] Array of [old, new] pairs
    # @param iterations [Integer] Number of times to run the test
    # @return [Hash] Timing results showing performance comparison
    def self.compare_with_ruby(str, replacements, iterations = 1000)
      require 'benchmark'
      
      # Clone inputs to ensure we have identical test cases
      ruby_str = str.dup
      c_str = str.dup
      
      # Convert replacements for Ruby's format if needed
      ruby_replacements = {}
      replacements.each { |old, new| ruby_replacements[old] = new }
      
      results = {}
      
      results[:ruby] = Benchmark.measure do
        iterations.times do
          ruby_replacements.each do |old, new|
            ruby_str.gsub!(old, new)
          end
        end
      end
      
      results[:c_extension] = Benchmark.measure do
        iterations.times do
          c_str = TableStringReplacer.batch_replace(c_str, replacements)
        end
      end
      
      results[:speedup] = results[:ruby].real / results[:c_extension].real
      
      results
    end
    
    # Tests thread safety by running multiple threads simultaneously
    #
    # @param threads [Integer] Number of threads to run concurrently
    # @param iterations [Integer] Number of replacements per thread
    # @return [Boolean] True if all threads completed successfully
    def self.test_thread_safety(threads = 10, iterations = 1000)
      replacements = [["old1", "new1"], ["old2", "new2"], ["old3", "new3"]]
      str = "This is old1 test string with old2 and old3 values"
      
      # Create arrays to store results
      results = []
      errors = []
      
      # Create and start threads
      thread_group = Array.new(threads) do |i|
        Thread.new do
          begin
            thread_result = nil
            iterations.times do
              thread_result = TableStringReplacer.batch_replace(str, replacements)
            end
            results[i] = thread_result
          rescue => e
            errors << e
          end
        end
      end
      
      # Wait for all threads to complete
      thread_group.each(&:join)
      
      # Check results
      success = errors.empty? && results.compact.size == threads
      success ? true : { success: false, errors: errors }
    end
  end
end 