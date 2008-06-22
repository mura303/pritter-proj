Gem::Specification.new do |s|
  s.name = %q{rfeedfinder}
  s.version = "0.9.11"

  s.specification_version = 1 if s.respond_to? :specification_version=

  s.required_rubygems_version = nil if s.respond_to? :required_rubygems_version=
  s.authors = ["Alexandre Girard"]
  s.cert_chain = nil
  s.date = %q{2007-10-29}
  s.description = %q{rFeedFinder uses RSS autodiscovery, Atom autodiscovery, spidering, URL correction, and Web service queries -- whatever it takes -- to find the feed.}
  s.email = %q{alx.girard@gmail.com}
  s.extra_rdoc_files = ["History.txt", "License.txt", "Manifest.txt", "README.txt", "website/index.txt"]
  s.files = ["History.txt", "License.txt", "Manifest.txt", "README.txt", "Rakefile", "lib/rfeedfinder.rb", "lib/rfeedfinder/version.rb", "scripts/txt2html", "setup.rb", "test/test_helper.rb", "test/test_rfeedfinder.rb", "website/index.html", "website/index.txt", "website/javascripts/rounded_corners_lite.inc.js", "website/stylesheets/screen.css", "website/template.rhtml"]
  s.has_rdoc = true
  s.homepage = %q{http://rfeedfinder.rubyforge.org}
  s.rdoc_options = ["--main", "README.txt"]
  s.require_paths = ["lib"]
  s.required_ruby_version = Gem::Requirement.new("> 0.0.0")
  s.rubyforge_project = %q{rfeedfinder}
  s.rubygems_version = %q{1.0.1}
  s.summary = %q{rFeedFinder uses RSS autodiscovery, Atom autodiscovery, spidering, URL correction, and Web service queries -- whatever it takes -- to find the feed.}
  s.test_files = ["test/test_helper.rb", "test/test_rfeedfinder.rb"]

  s.add_dependency(%q<hpricot>, [">= 0.6"])
  s.add_dependency(%q<htmlentities>, [">= 4.0.0"])
end
