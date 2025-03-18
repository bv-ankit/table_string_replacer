require "test_helper"

class TableStringReplacerBasicTest < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::TableStringReplacer::VERSION
  end

  def test_batch_replace
    content = "Hello world, hello ruby"
    replacements = [
      ["Hello", "Hi"],
      ["world", "everyone"],
      ["ruby", "Rails"]
    ]
    
    result = TableStringReplacer.batch_replace(content, replacements)
    assert_equal "Hi everyone, hello Rails", result
  end

  def test_replace_in_serialized
    serialized = 's:15:"http://old.com/";'
    old_str = "http://old.com"
    new_str = "https://new.com"
    
    result = TableStringReplacer.serialized_str_replace(serialized, old_str, new_str)
    # Just verify length and content are updated correctly
    assert_match(/s:16:.*"https:\/\/new.com\/";/, result)
  end

  def test_faster_than_ruby
    content = "a" * 10000 + "xyz" + "a" * 10000
    replacements = [["xyz", "abc"]]
    
    # We're not testing the actual result here, just that the method runs
    result = TableStringReplacer.faster_than_ruby?(content, replacements)
    assert [true, false].include?(result)
  end
end 