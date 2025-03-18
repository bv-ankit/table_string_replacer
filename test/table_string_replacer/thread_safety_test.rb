require 'test_helper'
require 'table_string_replacer/benchmarking'

class ThreadSafetyTest < Minitest::Test
  def test_concurrent_execution
    # Run 20 threads concurrently
    result = TableStringReplacer::Benchmarking.test_thread_safety(20, 5000)
    assert_equal true, result
  end
  
  def test_thread_local_variables
    # Test that different threads don't interfere with each other
    threads = 10.times.map do |i|
      Thread.new do
        # Each thread uses different replacements
        replacements = [["thread#{i}", "replaced#{i}"]]
        str = "This is a thread#{i} specific string"
        TableStringReplacer.batch_replace(str, replacements)
      end
    end
    
    results = threads.map(&:value)
    
    # Check that each thread got its own unique replacement
    10.times do |i|
      assert_equal "This is a replaced#{i} specific string", results[i]
    end
  end
end 