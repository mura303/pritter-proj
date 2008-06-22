Gem::Specification.new do |s|
  s.name = %q{simple-rss}
  s.version = "1.1"

  s.specification_version = 1 if s.respond_to? :specification_version=

  s.required_rubygems_version = nil if s.respond_to? :required_rubygems_version=
  s.authors = ["Lucas Carlson"]
  s.cert_chain = nil
  s.date = %q{2006-02-01}
  s.description = %q{A simple, flexible, extensible, and liberal RSS and Atom reader for Ruby. It is designed to be backwards compatible with the standard RSS parser, but will never do RSS generation.}
  s.email = %q{lucas@rufy.com}
  s.files = ["lib/simple-rss.rb", "test/base", "test/data", "test/test_helper.rb", "test/base/base_test.rb", "test/data/atom.xml", "test/data/not-rss.xml", "test/data/rss09.rdf", "test/data/rss20.xml", "LICENSE", "Rakefile", "README", "html/classes", "html/created.rid", "html/files", "html/fr_class_index.html", "html/fr_file_index.html", "html/fr_method_index.html", "html/index.html", "html/rdoc-style.css", "html/classes/SimpleRSS.html", "html/classes/SimpleRSS.src", "html/classes/SimpleRSSError.html", "html/classes/SimpleRSS.src/M000001.html", "html/classes/SimpleRSS.src/M000002.html", "html/classes/SimpleRSS.src/M000004.html", "html/classes/SimpleRSS.src/M000005.html", "html/classes/SimpleRSS.src/M000006.html", "html/classes/SimpleRSS.src/M000007.html", "html/classes/SimpleRSS.src/M000008.html", "html/files/lib", "html/files/README.html", "html/files/lib/simple-rss_rb.html"]
  s.has_rdoc = true
  s.homepage = %q{http://simple-rss.rubyforge.org/}
  s.require_paths = ["lib"]
  s.required_ruby_version = Gem::Requirement.new("> 0.0.0")
  s.rubygems_version = %q{1.0.1}
  s.summary = %q{A simple, flexible, extensible, and liberal RSS and Atom reader for Ruby. It is designed to be backwards compatible with the standard RSS parser, but will never do RSS generation.}
end
