Sequel.migration do
  change do
    create_table(:states) do
      primary_key :id
      String :name
    end

	alter_table(:games) do
		add_foreign_key :state_id, :states
    end
  end
end
