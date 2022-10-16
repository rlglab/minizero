
What is the dependency_graph_generator?
    Accoring to good coding practise, one should not have cyclic dependencies in the code.
    A cylic dependency is when one class/module/service that should encapsulate some
    functionality is directly or indirectly dependent on itself. A service is dependent on
    another service when its .cpp or .h file has an #include of another .h file. You could
    also have a dependency by other types of couplings, but the dependency_graph_generator
    is not capable of detecting these.
How to use the dependency_graph
    Step 1.
        Run in terminal:
        > python3 tools/dependency_graph_generator/dependency_graph_generator.py
    Step 2.
        A file "dependency_graph.graphml" has now been generated in
        tools/dependency_graph_generator/. Download this file by right clicking it.
    Step 3.
        Open website: https://www.yworks.com/yed-live/
        Click "Open an Existing Document"
        Click "From Local File"
        Open "dependency_graph.graphml" that you downloaded.
        Alternatively you can also drag and drop the file into the browser window.
    Step 7.
        Auto format the graph by clicking the yellow round button in the bottom
        left courner. The boxes will show classes/services and arrows will show
        dependencies.