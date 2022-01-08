CREATE DEFINER=`root`@`localhost` PROCEDURE `create_users`()
BEGIN
	declare id_nr int unsigned default 1;

	TRUNCATE users;
	TRUNCATE players;
	TRUNCATE supervisors;
    
	WHILE id_nr <= 108 DO
		INSERT INTO
			squid_game.users(id, last_name, first_name, city, weight, debt)
		VALUES
		(
			id_nr,
            concat("last_name", cast(id_nr as char)),
            concat("first_name", cast(id_nr as char)),
            concat("city", cast(id_nr as char)),
            (SELECT FLOOR(RAND()*(51))+50), -- 100-50+1 -> 100 max, 50 min
            (SELECT FLOOR(RAND()*(90001))+10000) -- 100000-10000+1 -> 100000 max, 10000 min
		);
		 SET id_nr=id_nr+1;
	END WHILE;
    SET id_nr=0;
END