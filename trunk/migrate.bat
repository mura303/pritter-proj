@echo Creating database...don't close this window.
ruby-win32\bin\ruby.exe ruby-win32\bin\rake db:migrate RAILS_ENV=production
pause
