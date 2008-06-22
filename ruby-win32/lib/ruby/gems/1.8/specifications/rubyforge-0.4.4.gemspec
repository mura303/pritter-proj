Gem::Specification.new do |s|
  s.name = %q{rubyforge}
  s.version = "0.4.4"

  s.specification_version = 1 if s.respond_to? :specification_version=

  s.required_rubygems_version = nil if s.respond_to? :required_rubygems_version=
  s.authors = ["Ara T Howard", "Ryan Davis", "Eric Hodel"]
  s.cert_chain = nil
  s.date = %q{2007-08-13}
  s.default_executable = %q{rubyforge}
  s.description = %q{A script which automates a limited set of rubyforge operations.  * Run 'rubyforge help' for complete usage. * Setup: For first time users AND upgrades to 0.4.0: * rubyforge setup (deletes your username and password, so run sparingly!) * edit ~/.rubyforge/user-config.yml * rubyforge config * For all rubyforge upgrades, run 'rubyforge config' to ensure you have latest. * Don't forget to login!  logging in will store a cookie in your .rubyforge directory which expires after a time.  always run the login command before any operation that requires authentication, such as uploading a package.}
  s.email = %q{ryand-ruby@zenspider.com}
  s.executables = ["rubyforge"]
  s.extra_rdoc_files = ["History.txt", "Manifest.txt", "README.txt"]
  s.files = ["History.txt", "Manifest.txt", "README.txt", "Rakefile", "bin/rubyforge", "lib/http-access2.rb", "lib/http-access2/cookie.rb", "lib/http-access2/http.rb", "lib/rubyforge.rb", "test/test_rubyforge.rb"]
  s.has_rdoc = true
  s.homepage = %q{http://rubyforge.org/projects/codeforpeople}
  s.rdoc_options = ["--main", "README.txt"]
  s.require_paths = ["lib"]
  s.required_ruby_version = Gem::Requirement.new("> 0.0.0")
  s.rubyforge_project = %q{codeforpeople}
  s.rubygems_version = %q{1.0.1}
  s.summary = %q{A script which automates a limited set of rubyforge operations.}
  s.test_files = ["test/test_rubyforge.rb"]
end
