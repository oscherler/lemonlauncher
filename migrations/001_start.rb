Sequel.migration do
  change do
    create_table(:games) do
      String :filename, :text=>true
      String :name, :text=>true, :null=>false
      String :genre, :default=>"Unknown", :text=>true, :null=>false
      String :clone_of, :text=>true
      String :manufacturer, :default=>"Unknown", :text=>true, :null=>false
      Integer :year, :default=>0, :null=>false
      DateTime :last_played
      String :params, :text=>true
      Integer :count, :default=>0, :null=>false
      TrueClass :favourite, :default=>false, :null=>false
      TrueClass :hide, :default=>false, :null=>false
      TrueClass :broken, :default=>false, :null=>false
      TrueClass :missing, :default=>true, :null=>false
      
      primary_key [:filename]
    end
  end
end
