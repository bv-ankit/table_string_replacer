# TableStringReplacer

A C extension for Ruby that provides optimized string replacement operations, with special handling for PHP serialized strings (commonly used in WordPress).

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'table_string_replacer'
```

And then execute:

    $ bundle install

Or install it yourself as:

    $ gem install table_string_replacer

## Usage

```ruby
require 'table_string_replacer'

# Simple batch replacement
content = "Hello world, hello ruby"
replacements = [
  ["Hello", "Hi"],
  ["world", "everyone"],
  ["ruby", "Rails"]
]

# Perform all replacements in a single pass
result = TableStringReplacer.batch_replace(content, replacements)
# => "Hi everyone, hello Rails"

# For WordPress serialized data
serialized = 's:15:"http://old.com/";'
old_str = "http://old.com"
new_str = "https://new.com"

# This will handle the PHP serialized string length correctly
result = TableStringReplacer.serialized_str_replace(serialized, old_str, new_str)
# => 's:16:"https://new.com/";'
```

## Performance

This gem is especially useful for batch operations on large strings where you need to make multiple replacements in a single pass. The C extension is typically much faster than Ruby's native string replacement for large inputs.

```ruby
# Check if the C extension is faster for your workload
TableStringReplacer.faster_than_ruby?(your_string, your_replacements)
```

## Development

After checking out the repo, run `bin/setup` to install dependencies. Then, run `rake test` to run the tests. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and the created tag, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Updating the Gem

1. Make your code changes and ensure all tests pass with `rake test`
2. Update the version number in `lib/table_string_replacer/version.rb` following [Semantic Versioning](https://semver.org/) principles
3. Update the CHANGELOG.md file with your changes
4. Commit your changes to git
5. Run `bundle exec rake release`, which will:
   - Create a git tag for the version
   - Push git commits and tags
   - Build the gem and push it to RubyGems.org

Alternatively, you can do these steps manually:
1. Build the gem: `gem build table_string_replacer.gemspec`
2. Push to RubyGems: `gem push table_string_replacer-x.x.x.gem` (where x.x.x is the version number)

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/bv-ankit/table_string_replacer.

## License

The gem is available as open source under the terms of the [MIT License](https://opensource.org/licenses/MIT). 