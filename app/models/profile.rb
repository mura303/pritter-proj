class Profile < ActiveRecord::Base
  has_many :tweets
end
