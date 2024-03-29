Gem::Specification.new do |s|
  s.name = %q{hpricot}
  s.version = "0.6"
  s.platform = %q{mswin32}

  s.specification_version = 1 if s.respond_to? :specification_version=

  s.required_rubygems_version = nil if s.respond_to? :required_rubygems_version=
  s.authors = ["why the lucky stiff"]
  s.cert_chain = nil
  s.date = %q{2007-06-15}
  s.description = %q{a swift, liberal HTML parser with a fantastic library}
  s.email = %q{why@ruby-lang.org}
  s.extra_rdoc_files = ["README", "CHANGELOG", "COPYING"]
  s.files = ["CHANGELOG", "COPYING", "README", "Rakefile", "test/files", "test/test_preserved.rb", "test/test_paths.rb", "test/load_files.rb", "test/test_xml.rb", "test/test_parser.rb", "test/test_alter.rb", "test/test_builder.rb", "test/files/why.xml", "test/files/boingboing.html", "test/files/uswebgen.html", "test/files/immob.html", "test/files/week9.html", "test/files/utf8.html", "test/files/basic.xhtml", "test/files/cy0.html", "test/files/tenderlove.html", "test/files/pace_application.html", "lib/hpricot", "lib/hpricot.rb", "lib/i686-linux", "lib/hpricot/builder.rb", "lib/hpricot/htmlinfo.rb", "lib/hpricot/xchar.rb", "lib/hpricot/inspect.rb", "lib/hpricot/modules.rb", "lib/hpricot/parse.rb", "lib/hpricot/tag.rb", "lib/hpricot/traverse.rb", "lib/hpricot/elements.rb", "lib/hpricot/tags.rb", "lib/hpricot/blankslate.rb", "extras/mingw-rbconfig.rb", "ext/hpricot_scan/hpricot_scan.h", "ext/hpricot_scan/HpricotScanService.java", "ext/hpricot_scan/hpricot_scan.c", "ext/hpricot_scan/extconf.rb", "ext/hpricot_scan/hpricot_common.rl", "ext/hpricot_scan/hpricot_scan.rl", "ext/hpricot_scan/hpricot_scan.java.rl", "lib/i686-linux/hpricot_scan.so"]
  s.has_rdoc = true
  s.homepage = %q{http://code.whytheluckystiff.net/hpricot/}
  s.rdoc_options = ["--quiet", "--title", "The Hpricot Reference", "--main", "README", "--inline-source"]
  s.require_paths = ["lib/i686-linux", "lib"]
  s.required_ruby_version = Gem::Requirement.new("> 0.0.0")
  s.rubygems_version = %q{1.0.1}
  s.summary = %q{a swift, liberal HTML parser with a fantastic library}
end
