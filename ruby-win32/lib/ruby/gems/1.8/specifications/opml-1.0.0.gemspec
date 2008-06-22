Gem::Specification.new do |s|
  s.name = %q{opml}
  s.version = "1.0.0"

  s.specification_version = 2 if s.respond_to? :specification_version=

  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
  s.authors = ["Joshua Peek"]
  s.autorequire = %q{opml}
  s.date = %q{2007-12-18}
  s.email = %q{josh@joshpeek.com}
  s.files = ["MIT-LICENSE", "Rakefile", "README", "lib/opml.rb", "spec/files", "spec/files/playlist.opml", "spec/files/presentation.opml", "spec/files/specification.opml", "spec/opml_spec.rb"]
  s.homepage = %q{http://rubyforge.org/projects/opml/}
  s.require_paths = ["lib"]
  s.rubygems_version = %q{1.0.1}
  s.summary = %q{A simple wrapper for parsing OPML files.}
end
