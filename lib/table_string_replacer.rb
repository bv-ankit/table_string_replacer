require "table_string_replacer/version"
# Load the compiled C extension
require "table_string_replacer/table_string_replacer"

module TableStringReplacer
	class Error < StandardError; end
	
	# This module provides high-performance string replacements
	# optimized for WordPress serialized data.
	#
	# All methods are thread-safe and can be called concurrently
	# from multiple threads without any issues.
	
	# Returns true if a benchmark shows this gem is faster than 
	# native Ruby string replacement for your workload
	#
	# @param str [String] Sample input string 
	# @param replacements [Array<Array<String, String>>] Array of [old, new] pairs
	# @return [Boolean] true if the C extension is faster
	def self.faster_than_ruby?(str, replacements)
		start_time = Time.now
		100.times { str.gsub(replacements.first[0], replacements.first[1]) }
		ruby_time = Time.now - start_time
		
		start_time = Time.now
		100.times { TableStringReplacer.batch_replace(str, replacements) }
		c_time = Time.now - start_time
		
		c_time < ruby_time
	end
end 