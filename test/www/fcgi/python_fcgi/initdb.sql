drop table if exists `users`;

create table `users` (
    user varchar(20) primary key,
    password varchar(20)
);

insert into `users` (`user`, `password`)
values('root', '123456789');

