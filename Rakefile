require "bundler/gem_tasks"
require "rake/testtask"
require "rake/extensiontask"

Rake::TestTask.new(:test) do |t|
  t.libs << "test"
  t.libs << "lib"
  t.test_files = FileList["test/**/*_test.rb"]
end

Rake::ExtensionTask.new("table_string_replacer") do |ext|
  ext.lib_dir = "lib/table_string_replacer"
end

task :default => [:compile, :test] 