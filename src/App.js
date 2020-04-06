import React from 'react';
import './App.css';
function App() {
  return (

    <div className="App">
      <nav>
        Este simulador no es oficial, es un proyecto para ayudar con la asignatura
        </nav>
      <header className="Simulador 19-20">
        <h1>Simulador 2019-2020</h1>
      </header>
      <body>
        <form>
          <label>
            <div>Programa 1:<input type="text" name="p1" />Tiempo 1:<input type="text" name="t1" /></div>
            <div>Programa 2:<input type="text" name="p2" />Tiempo 2:<input type="text" name="t2" /></div>
            <div> Programa 3:<input type="text" name="p3" />Tiempo 3:<input type="text" name="t3" /></div>
          </label>
          <input type="submit" value="Submit" />
        </form>
      </body>
      <footer>
        Version 3
      </footer>
    </div>
  );
}

export default App;
