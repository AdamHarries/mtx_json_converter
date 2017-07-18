

#include "options.h"

#include "sparse_matrix.h"

int main(int argc, char *argv[])
{
	OptParser op(
      "Converter from matrix-market format files to ICFP challenge JSON files");
  	auto opt_input_file = op.addOption<std::string>({'m', "matrixfile", "Input matrix file"});

  	op.parse(argc, argv);

  	const std::string input_filename = opt_input_file->get();

 	std::cout << "input_filename " << input_filename << std::endl;

 	SparseMatrix matrix(input_filename);

 	std::cout<<"Got "<<matrix.nonZeros()<<" edges"<<std::endl;

 	// begin dumping json
 	{
 		std::cout<<"{";
 		{
 			// dump an array of nodes: 
 			std::cout<<"\"nodes\": [";
 			{
 				for(int i = 0;i<matrix.nonZeros(); i++){
 					if(i!=0){
 						std::cout<<", ";
 					}
 					std::cout<<"{\"id\": "<<i<<"}";
 				}
 			}
 			std::cout<<"],";

 			// dump an array of edges:
 			std::cout<<"\"edges\": [";
 			{
 				// iterate over the non-zero matrix entries
 				bool defer = false;
 				for(auto entry: matrix.getEntries()){
 					auto v1 = std::get<0>(entry);
 					auto v2 = std::get<1>(entry);
 					if(defer){
 						std::cout<<", ";
 					}
 					defer = true;
 					// entry
 					std::cout<<"{\"source\": "<<v1<<", \"target\": "<<v2<<"}";
 				}
 			}
 			std::cout<<"]";
 		}
 		std::cout<<"}";
 	}
 	std::cout<<std::endl;

	/* code */ 
	return 0;
}