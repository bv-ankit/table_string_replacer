require_relative 'lib/table_string_replacer/version'

Gem::Specification.new do |spec|
	spec.name          = "table_string_replacer"
	spec.version       = TableStringReplacer::VERSION
	spec.authors       = ["Your Name"]
	spec.email         = ["your.email@example.com"]

	spec.summary       = "Fast string replacement operations with PHP serialization support"
	spec.description   = "A C extension that provides optimized string replacement, specially handling PHP serialized strings"
	spec.homepage      = "https://github.com/yourusername/table_string_replacer"
	spec.license       = "MIT"
	spec.required_ruby_version = Gem::Requirement.new(">= 2.5.0")

	spec.metadata["homepage_uri"] = spec.homepage
	spec.metadata["source_code_uri"] = spec.homepage
	spec.metadata["changelog_uri"] = "#{spec.homepage}/blob/master/CHANGELOG.md"

	# Specify which files should be added to the gem when it is released.
	spec.files = Dir.glob(%w[
		lib/**/*.rb
		ext/**/*.{c,h,rb}
		*.md
		LICENSE.txt
	])
	spec.bindir        = "bin"
	spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
	spec.require_paths = ["lib"]
	spec.extensions    = ["ext/table_string_replacer/extconf.rb"]
end 