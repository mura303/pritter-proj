Gem::Specification.new do |s|
  s.name = %q{win32-service}
  s.version = "0.5.2"
  s.platform = %q{mswin32}

  s.specification_version = 1 if s.respond_to? :specification_version=

  s.required_rubygems_version = nil if s.respond_to? :required_rubygems_version=
  s.authors = ["Daniel J. Berger"]
  s.cert_chain = nil
  s.date = %q{2006-12-05}
  s.description = %q{An interface for MS Windows services}
  s.email = %q{djberg96@gmail.com}
  s.extra_rdoc_files = ["CHANGES", "README", "MANIFEST", "doc/service.txt", "doc/daemon.txt"]
  s.files = ["doc/daemon.txt", "doc/service.txt", "test/tc_daemon.rb", "test/tc_service.rb", "lib/win32/service.c", "lib/win32/service.h", "lib/win32/service.so", "CHANGES", "README", "MANIFEST"]
  s.has_rdoc = true
  s.homepage = %q{http://www.rubyforge.org/projects/win32utils}
  s.require_paths = ["lib"]
  s.required_ruby_version = Gem::Requirement.new(">= 1.8.2")
  s.rubyforge_project = %q{win32utils}
  s.rubygems_version = %q{1.0.1}
  s.summary = %q{An interface for MS Windows services}
  s.test_files = ["test/tc_service.rb"]
end
