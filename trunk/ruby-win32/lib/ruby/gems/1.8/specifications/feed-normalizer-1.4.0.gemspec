Gem::Specification.new do |s|
  s.name = %q{feed-normalizer}
  s.version = "1.4.0"

  s.specification_version = 1 if s.respond_to? :specification_version=

  s.required_rubygems_version = nil if s.respond_to? :required_rubygems_version=
  s.authors = ["Andrew A. Smith"]
  s.cert_chain = nil
  s.date = %q{2007-07-10}
  s.description = %q{An extensible Ruby wrapper for Atom and RSS parsers.  Feed normalizer wraps various RSS and Atom parsers, and returns a single unified object graph, regardless of the underlying feed format.}
  s.email = %q{andy@tinnedfruit.org}
  s.extra_rdoc_files = ["History.txt", "License.txt", "Manifest.txt", "README.txt"]
  s.files = ["History.txt", "License.txt", "Manifest.txt", "Rakefile", "README.txt", "lib/feed-normalizer.rb", "lib/html-cleaner.rb", "lib/parsers/rss.rb", "lib/parsers/simple-rss.rb", "lib/structures.rb", "test/data/atom03.xml", "test/data/atom10.xml", "test/data/rdf10.xml", "test/data/rss20.xml", "test/data/rss20diff.xml", "test/data/rss20diff_short.xml", "test/test_all.rb", "test/test_feednormalizer.rb", "test/test_htmlcleaner.rb"]
  s.has_rdoc = true
  s.homepage = %q{http://feed-normalizer.rubyforge.org/}
  s.rdoc_options = ["--main", "README.txt"]
  s.require_paths = ["lib"]
  s.required_ruby_version = Gem::Requirement.new("> 0.0.0")
  s.rubyforge_project = %q{feed-normalizer}
  s.rubygems_version = %q{1.0.1}
  s.summary = %q{Extensible Ruby wrapper for Atom and RSS parsers}
  s.test_files = ["test/test_all.rb"]

  s.add_dependency(%q<simple-rss>, [">= 1.1"])
  s.add_dependency(%q<hpricot>, [">= 0.6"])
  s.add_dependency(%q<hoe>, [">= 1.2.1"])
end
