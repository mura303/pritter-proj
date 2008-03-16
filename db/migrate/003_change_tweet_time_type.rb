class ChangeTweetTimeType < ActiveRecord::Migration
  def self.up
    change_column( :tweets, :created_on, :datetime )
  end


  def self.down
    change_column( :tweets, :created_on, :time )
  end
end
