import sqlalchemy
from slavealloc.data import model


def setup(db_url, db_kwargs={}):
    engine = sqlalchemy.create_engine(db_url, **db_kwargs)
    model.metadata.bind = engine
